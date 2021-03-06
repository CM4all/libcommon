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

#include "Registry.hxx"
#include "ExitListener.hxx"
#include "event/Loop.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/StringFormat.hxx"

#include <cassert>
#include <chrono>
#include <string>

#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>

static constexpr auto child_kill_timeout = std::chrono::minutes(1);

struct ChildProcessRegistry::ChildProcess
	: boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>
{
	const Logger logger;

	const pid_t pid;

	const std::string name;

	/**
	 * The time when this child process was started (registered in
	 * this library).
	 */
	const std::chrono::steady_clock::time_point start_time;

	ExitListener *listener;

	/**
	 * This timer is set up by child_kill_signal().  If the child
	 * process hasn't exited after a certain amount of time, we send
	 * SIGKILL.
	 */
	CoarseTimerEvent kill_timeout_event;

	ChildProcess(EventLoop &event_loop,
		     pid_t _pid, const char *_name,
		     ExitListener *_listener) noexcept;

	auto &GetEventLoop() noexcept {
		return kill_timeout_event.GetEventLoop();
	}

	void OnExit(int status, const struct rusage &rusage) noexcept;

	void KillTimeoutCallback() noexcept;
};

inline bool
ChildProcessRegistry::CompareChildProcess::operator()(const ChildProcess &a,
						      const ChildProcess &b) const noexcept
{
	return a.pid < b.pid;
}

inline bool
ChildProcessRegistry::CompareChildProcess::operator()(const ChildProcess &a,
						      pid_t b) const noexcept
{
	return a.pid < b;
}

inline bool
ChildProcessRegistry::CompareChildProcess::operator()(pid_t a,
						      const ChildProcess &b) const noexcept
{
	return a < b.pid;
}

static std::string
MakeChildProcessLogDomain(unsigned pid, const char *name) noexcept
{
	return StringFormat<64>("spawn:%u:%s", pid, name).c_str();
}

ChildProcessRegistry::ChildProcess::ChildProcess(EventLoop &_event_loop,
						 pid_t _pid, const char *_name,
						 ExitListener *_listener) noexcept
	:logger(MakeChildProcessLogDomain(_pid, _name)),
	 pid(_pid), name(_name),
	 start_time(_event_loop.SteadyNow()),
	 listener(_listener),
	 kill_timeout_event(_event_loop, BIND_THIS_METHOD(KillTimeoutCallback))
{
	logger(5, "added child process");
}

static constexpr double
timeval_to_double(const struct timeval &tv) noexcept
{
	return tv.tv_sec + tv.tv_usec / 1000000.;
}

void
ChildProcessRegistry::ChildProcess::OnExit(int status,
					   const struct rusage &rusage) noexcept
{
	const int exit_status = WEXITSTATUS(status);
	if (WIFSIGNALED(status)) {
		int level = 1;
		if (!WCOREDUMP(status) && WTERMSIG(status) == SIGTERM)
			level = 4;

		logger(level, "died from signal ",
		       WTERMSIG(status),
		       WCOREDUMP(status) ? " (core dumped)" : "");
	} else if (exit_status == 0)
		logger(5, "exited with success");
	else
		logger(2, "exited with status ", exit_status);

	const auto duration = GetEventLoop().SteadyNow() - start_time;
	const auto duration_f = std::chrono::duration_cast<std::chrono::duration<double>>(duration);

	logger.Format(6, "stats: %1.3fs elapsed, %1.3fs user, %1.3fs sys, %ld/%ld faults, %ld/%ld switches",
		      duration_f.count(),
		      timeval_to_double(rusage.ru_utime),
		      timeval_to_double(rusage.ru_stime),
		      rusage.ru_minflt, rusage.ru_majflt,
		      rusage.ru_nvcsw, rusage.ru_nivcsw);

	if (listener != nullptr)
		listener->OnChildProcessExit(status);
}

inline void
ChildProcessRegistry::ChildProcess::KillTimeoutCallback() noexcept
{
	logger(3, "sending SIGKILL to due to timeout");

	if (kill(pid, SIGKILL) < 0)
		logger(1, "failed to kill child process: ", strerror(errno));
}

ChildProcessRegistry::ChildProcessRegistry(EventLoop &_event_loop) noexcept
	:logger("spawn"), event_loop(_event_loop),
	 sigchld_event(event_loop, SIGCHLD, BIND_THIS_METHOD(OnSigChld))
{
	sigchld_event.Enable();
}

ChildProcessRegistry::~ChildProcessRegistry() noexcept = default;

inline ChildProcessRegistry::ChildProcessSet::iterator
ChildProcessRegistry::FindByPid(pid_t pid) noexcept
{
	return children.find(pid, children.key_comp());
}

void
ChildProcessRegistry::Clear() noexcept
{
	children.clear_and_dispose(DeleteDisposer());

	CheckVolatileEvent();
}

void
ChildProcessRegistry::Add(pid_t pid, const char *name,
			  ExitListener *listener) noexcept
{
	assert(name != nullptr);

	if (volatile_event && IsEmpty())
		sigchld_event.Enable();

	auto child = new ChildProcess(event_loop, pid, name, listener);

	children.insert(*child);
}

void
ChildProcessRegistry::SetExitListener(pid_t pid,
				      ExitListener *listener) noexcept
{
	assert(pid > 0);
	assert(listener != nullptr);

	auto i = FindByPid(pid);
	assert(i != children.end());
	auto &child = *i;

	assert(child.listener == nullptr);
	child.listener = listener;
}

void
ChildProcessRegistry::Kill(pid_t pid, int signo) noexcept
{
	auto i = FindByPid(pid);
	assert(i != children.end());
	auto *child = &*i;

	logger(5, "sending ", strsignal(signo));

	assert(child->listener != nullptr);
	child->listener = nullptr;

	if (kill(pid, signo) < 0) {
		logger(1, "failed to kill child process: ", strerror(errno));

		/* if we can't kill the process, we can't do much, so let's
		   just ignore the process from now on and don't let it delay
		   the shutdown */
		children.erase_and_dispose(i, DeleteDisposer{});
		CheckVolatileEvent();
		return;
	}

	child->kill_timeout_event.Schedule(child_kill_timeout);
}

void
ChildProcessRegistry::Kill(pid_t pid) noexcept
{
	Kill(pid, SIGTERM);
}

void
ChildProcessRegistry::OnExit(pid_t pid, int status,
			     const struct rusage &rusage) noexcept
{
	auto i = FindByPid(pid);
	if (i == children.end())
		return;

	auto *child = &*i;
	children.erase(i);
	child->OnExit(status, rusage);
	delete child;
}


void
ChildProcessRegistry::OnSigChld(int) noexcept
{
	pid_t pid;
	int status;

	struct rusage rusage;
	while ((pid = wait4(-1, &status, WNOHANG, &rusage)) > 0) {
		OnExit(pid, status, rusage);
	}

	CheckVolatileEvent();
}
