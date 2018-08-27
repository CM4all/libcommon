/*
 * Copyright 2007-2018 Content Management AG
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
#include "util/DeleteDisposer.hxx"
#include "util/StringFormat.hxx"

#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>

static constexpr struct timeval child_kill_timeout = {
	.tv_sec = 60,
	.tv_usec = 0,
};

static std::string
MakeChildProcessLogDomain(unsigned pid, const char *name)
{
	return StringFormat<64>("spawn:%u:%s", pid, name).c_str();
}

ChildProcessRegistry::ChildProcess::ChildProcess(EventLoop &_event_loop,
						 pid_t _pid, const char *_name,
						 ExitListener *_listener)
	:logger(MakeChildProcessLogDomain(_pid, _name)),
	 pid(_pid), name(_name),
	 start_time(std::chrono::steady_clock::now()),
	 listener(_listener),
	 kill_timeout_event(_event_loop, BIND_THIS_METHOD(KillTimeoutCallback))
{
	logger(5, "added child process");
}

static constexpr double
timeval_to_double(const struct timeval &tv)
{
	return tv.tv_sec + tv.tv_usec / 1000000.;
}

void
ChildProcessRegistry::ChildProcess::OnExit(int status,
					   const struct rusage &rusage)
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

	const auto duration = std::chrono::steady_clock::now() - start_time;
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
ChildProcessRegistry::ChildProcess::KillTimeoutCallback()
{
	logger(3, "sending SIGKILL to due to timeout");

	if (kill(pid, SIGKILL) < 0)
		logger(1, "failed to kill child process: ", strerror(errno));
}

ChildProcessRegistry::ChildProcessRegistry(EventLoop &_event_loop)
	:logger("spawn"), event_loop(_event_loop),
	 sigchld_event(event_loop, SIGCHLD, BIND_THIS_METHOD(OnSigChld))
{
	sigchld_event.Enable();
}

void
ChildProcessRegistry::Clear()
{
	children.clear_and_dispose(DeleteDisposer());

	CheckVolatileEvent();
}

void
ChildProcessRegistry::Add(pid_t pid, const char *name, ExitListener *listener)
{
	assert(name != nullptr);

	if (volatile_event && IsEmpty())
		sigchld_event.Enable();

	auto child = new ChildProcess(event_loop, pid, name, listener);

	children.insert(*child);
}

void
ChildProcessRegistry::SetExitListener(pid_t pid, ExitListener *listener)
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
ChildProcessRegistry::Kill(pid_t pid, int signo)
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
		Remove(i);
		delete child;
		CheckVolatileEvent();
		return;
	}

	child->kill_timeout_event.Add(child_kill_timeout);
}

void
ChildProcessRegistry::Kill(pid_t pid)
{
	Kill(pid, SIGTERM);
}

void
ChildProcessRegistry::OnExit(pid_t pid, int status,
			     const struct rusage &rusage)
{
	auto i = FindByPid(pid);
	if (i == children.end())
		return;

	auto *child = &*i;
	Remove(i);
	child->OnExit(status, rusage);
	delete child;
}


void
ChildProcessRegistry::OnSigChld(int)
{
	pid_t pid;
	int status;

	struct rusage rusage;
	while ((pid = wait4(-1, &status, WNOHANG, &rusage)) > 0) {
		OnExit(pid, status, rusage);
	}

	CheckVolatileEvent();
}
