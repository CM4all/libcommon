/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "CgroupKill.hxx"
#include "CgroupState.hxx"
#include "system/Error.hxx"
#include "system/LinuxFD.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <array>
#include <span>

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/inotify.h>

static UniqueFileDescriptor
OpenUnifiedCgroup(const CgroupState &state, const char *name)
{
	assert(state.IsEnabled());

	return OpenPath(state.GetUnifiedMount(), name);
}

static UniqueFileDescriptor
OpenUnifiedCgroup(const CgroupState &state, const char *name,
		  const char *session)
{
	return session != nullptr
		? OpenPath(OpenUnifiedCgroup(state, name), session)
		: OpenUnifiedCgroup(state, name);
}

static bool
IsPopulated(FileDescriptor fd) noexcept
{
	char buffer[4096];
	ssize_t nbytes = pread(fd.Get(), buffer, sizeof(buffer) - 1, 0);
	if (nbytes <= 0)
		return false;

	buffer[nbytes] = 0;
	return strstr(buffer, "populated 0") == nullptr;
}

static int
AddWatchOrThrow(FileDescriptor fd, const char *pathname, uint32_t mask)
{
	int wd = inotify_add_watch(fd.Get(), pathname, mask);
	if (wd < 0)
		throw FormatErrno("inotify_add_watch('%s') failed", pathname);

	return wd;
}

static std::string
MakeCgroupEventsPath(const std::string &cgroup_path) noexcept
{
	return cgroup_path + "/cgroup.events";
}

static UniqueFileDescriptor
OpenCgroupKill(const CgroupState &state, FileDescriptor cgroup_fd)
{
	UniqueFileDescriptor fd;

	if (state.cgroup_kill)
		fd.Open(cgroup_fd, "cgroup.kill", O_WRONLY);

	return fd;
}

inline
CgroupKill::CgroupKill(EventLoop &event_loop,
		       const CgroupState &state,
		       const std::string &cgroup_path,
		       UniqueFileDescriptor &&cgroup_fd,
		       CgroupKillHandler &_handler)
	:handler(_handler),
	 inotify_fd(CreateInotify()),
	 inotify_event(event_loop, BIND_THIS_METHOD(OnInotifyEvent),
		       inotify_fd),
	 cgroup_events_fd(OpenReadOnly(cgroup_fd, "cgroup.events")),
	 cgroup_procs_fd(OpenReadOnly(cgroup_fd, "cgroup.procs")),
	 cgroup_kill_fd(OpenCgroupKill(state, cgroup_fd)),
	 send_term_event(event_loop, BIND_THIS_METHOD(OnSendTerm)),
	 send_kill_event(event_loop, BIND_THIS_METHOD(OnSendKill)),
	 timeout_event(event_loop, BIND_THIS_METHOD(OnTimeout))
{
	// TODO: initialize inotify only if cgroup is populated currently
	AddWatchOrThrow(inotify_fd,
			MakeCgroupEventsPath(cgroup_path).c_str(),
			IN_MODIFY);

	inotify_event.ScheduleRead();
	send_term_event.Schedule();
}

static std::string
MakeCgroupUnifiedPath(const CgroupState &state, const char *name,
		      const char *session) noexcept
{
	auto result = state.GetUnifiedMountPath() + state.group_path + "/" + name;
	if (session != nullptr) {
		result.push_back('/');
		result += session;
	}

	return result;
}

CgroupKill::CgroupKill(EventLoop &event_loop, const CgroupState &state,
		       const char *name, const char *session,
		       CgroupKillHandler &_handler)
	:CgroupKill(event_loop, state,
		    MakeCgroupUnifiedPath(state, name, session),
		    OpenUnifiedCgroup(state, name, session),
		    _handler)
{
}

bool
CgroupKill::CheckPopulated() noexcept
{
	if (IsPopulated(cgroup_events_fd))
		return true;

	Disable();
	handler.OnCgroupKill();
	return false;
}

void
CgroupKill::OnInotifyEvent(unsigned) noexcept
{
	uint8_t buffer[1024];
	ssize_t nbytes = inotify_fd.Read(buffer, sizeof(buffer));
	if (nbytes < 0) {
		const int e = errno;
		if (e == EAGAIN)
			return;

		Disable();
		handler.OnCgroupKillError(std::make_exception_ptr(MakeErrno(e, "Read from inotify failed")));
		return;
	}

	CheckPopulated();
}

static size_t
LoadCgroupPids(FileDescriptor cgroup_procs_fd, std::span<pid_t> pids)
{
	char buffer[8192];

	ssize_t nbytes = pread(cgroup_procs_fd.Get(),
			       buffer, sizeof(buffer) - 1, 0);
	if (nbytes < 0)
		throw MakeErrno("Reading cgroup.procs failed");

	buffer[nbytes] = 0;

	const char *s = buffer;
	size_t n = 0;

	while (n < pids.size()) {
		char *endptr;
		auto value = strtoul(s, &endptr, 10);
		if (*endptr == 0)
			break;

		if (endptr > s && *endptr == '\n')
			pids[n++] = value;

		s = endptr + 1;
	}

	return n;
}

static void
KillCgroup(FileDescriptor cgroup_procs_fd, int sig)
{
	std::array<pid_t, 256> pids;
	size_t n = LoadCgroupPids(cgroup_procs_fd, pids);
	if (n == 0)
		throw std::runtime_error("Populated cgroup has no tasks");

	for (size_t i = 0; i < n; ++i)
		// TODO: check for kill() failures?
		kill(pids[i], sig);
}

void
CgroupKill::OnSendTerm() noexcept
{
	if (!CheckPopulated())
		return;

	try {
		KillCgroup(cgroup_procs_fd, SIGTERM);
	} catch (...) {
		Disable();
		handler.OnCgroupKillError(std::current_exception());
		return;
	}

	send_kill_event.Schedule(std::chrono::seconds(10));
}

void
CgroupKill::OnSendKill() noexcept
{
	if (!CheckPopulated())
		return;

	try {
		if (cgroup_kill_fd.IsDefined() &&
		    cgroup_kill_fd.Write("1", 1) == 1)
			return;

		KillCgroup(cgroup_procs_fd, SIGKILL);
	} catch (...) {
		Disable();
		handler.OnCgroupKillError(std::current_exception());
		return;
	}

	timeout_event.Schedule(std::chrono::seconds(10));
}

void
CgroupKill::OnTimeout() noexcept
{
	if (!CheckPopulated())
		return;

	Disable();
	handler.OnCgroupKillError(std::make_exception_ptr(std::runtime_error("cgroup did not exit after SIGKILL")));
}
