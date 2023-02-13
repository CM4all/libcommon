// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupKill.hxx"
#include "CgroupState.hxx"
#include "system/Error.hxx"
#include "io/Open.hxx"
#include "io/ScopeChdir.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <array>
#include <span>

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

static UniqueFileDescriptor
OpenUnifiedCgroup(const CgroupState &state, const char *name)
{
	assert(state.IsEnabled());

	return OpenPath(state.GetUnifiedGroupMount(), name);
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
		       FileDescriptor cgroup_fd,
		       CgroupKillHandler &_handler)
	:handler(_handler),
	 inotify_event(event_loop, *this),
	 cgroup_events_fd(OpenReadOnly(cgroup_fd, "cgroup.events")),
	 cgroup_procs_fd(OpenReadOnly(cgroup_fd, "cgroup.procs")),
	 cgroup_kill_fd(OpenCgroupKill(state, cgroup_fd)),
	 send_term_event(event_loop, BIND_THIS_METHOD(OnSendTerm)),
	 send_kill_event(event_loop, BIND_THIS_METHOD(OnSendKill)),
	 timeout_event(event_loop, BIND_THIS_METHOD(OnTimeout))
{
	// TODO: initialize inotify only if cgroup is populated currently
	{
		const ScopeChdir scope_chdir{cgroup_fd};
		inotify_event.AddModifyWatch("cgroup.events");
	}

	send_term_event.Schedule();
}

CgroupKill::CgroupKill(EventLoop &event_loop, const CgroupState &state,
		       const char *name, const char *session,
		       CgroupKillHandler &_handler)
	:CgroupKill(event_loop, state,
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
CgroupKill::OnInotify(int, unsigned, const char *)
{
	CheckPopulated();
}

void
CgroupKill::OnInotifyError(std::exception_ptr error) noexcept
{
	handler.OnCgroupKillError(std::move(error));
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
