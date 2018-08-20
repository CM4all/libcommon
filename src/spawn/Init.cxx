/*
 * Copyright 2007-2017 Content Management AG
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

#include "Init.hxx"
#include "SeccompFilter.hxx"
#include "system/Error.hxx"
#include "system/ProcessName.hxx"
#include "system/CapabilityState.hxx"
#include "io/FileDescriptor.hxx"
#include "util/Macros.hxx"

#include <assert.h>
#include <sys/wait.h>
#include <sys/signalfd.h>
#include <sys/capability.h>
#include <sched.h>
#include <stdlib.h>
#include <dirent.h>

static sigset_t init_signal_mask;
static FileDescriptor init_signal_fd = FileDescriptor::Undefined();

static void
CloseAllFiles()
{
	auto *d = opendir("/proc/self/fd");
	if (d != nullptr) {
		const int except = dirfd(d);
		while (auto *e = readdir(d)) {
			const char *name = e->d_name;
			char *endptr;
			auto fd = strtoul(name, &endptr, 10);
			if (endptr > name && *endptr == 0 && fd > 2 && int(fd) != except)
				close(fd);
		}

		closedir(d);
	} else {
		for (int i = 3; i < 1024; ++i)
			close(i);
	}
}

pid_t
SpawnInitFork()
{
	assert(!init_signal_fd.IsDefined());

	sigemptyset(&init_signal_mask);
	sigaddset(&init_signal_mask, SIGINT);
	sigaddset(&init_signal_mask, SIGQUIT);
	sigaddset(&init_signal_mask, SIGTERM);
	sigaddset(&init_signal_mask, SIGCHLD);

	sigprocmask(SIG_BLOCK, &init_signal_mask, nullptr);

	pid_t pid = fork();
	if (pid < 0)
		throw MakeErrno("fork() failed");

	if (pid == 0) {
		sigprocmask(SIG_UNBLOCK, &init_signal_mask, nullptr);
	} else {
		SetProcessName("init");

		CloseAllFiles();

		if (!init_signal_fd.CreateSignalFD(&init_signal_mask, false))
			throw MakeErrno("signalfd() failed");
	}

	return pid;
}

static void
DropCapabilities()
{
	static constexpr cap_value_t keep_caps[] = {
		/* needed to forward received signals to the main child
		   process (running under a different uid) */
		CAP_KILL,
	};

	CapabilityState state = CapabilityState::Empty();
	state.SetFlag(CAP_EFFECTIVE, {keep_caps, ARRAY_SIZE(keep_caps)}, CAP_SET);
	state.SetFlag(CAP_PERMITTED, {keep_caps, ARRAY_SIZE(keep_caps)}, CAP_SET);
	state.Install();
}

/**
 * Install a very strict seccomp filter which allows only very few
 * system calls.
 */
static void
LimitSysCalls(FileDescriptor read_fd, pid_t kill_pid)
{
	using Seccomp::Arg;

	Seccomp::Filter sf(SCMP_ACT_KILL);
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(read), Arg(0) == read_fd.Get());
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(wait4));
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(waitid));
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(kill), Arg(0) == kill_pid);
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(exit_group));
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(exit));
	sf.Load();
}

int
SpawnInit(pid_t child_pid, bool remain)
{
	if (geteuid() == 0)
		DropCapabilities();

	LimitSysCalls(init_signal_fd, child_pid);

	int last_status = EXIT_SUCCESS;

	while (true) {
		/* reap zombies */

		while (true) {
			int status;
			pid_t pid = waitpid(-1, &status, WNOHANG);
			if (pid < 0) {
				if (remain && errno == ECHILD)
					/* no more child processes: keep running */
					break;

				return last_status;
			}

			if (pid == 0)
				break;

			if (pid == child_pid) {
				child_pid = -1;

				if (WIFEXITED(status))
					last_status = WEXITSTATUS(status);
				else
					last_status = EXIT_FAILURE;
			}
		}

		/* receive signals */

		struct signalfd_siginfo info;
		ssize_t nbytes = init_signal_fd.Read(&info, sizeof(info));
		if (nbytes <= 0)
			return EXIT_FAILURE;

		switch (info.ssi_signo) {
		case SIGINT:
		case SIGQUIT:
		case SIGTERM:
			/* forward signals to the main child */
			if (child_pid > 0)
				kill(child_pid, info.ssi_signo);
			else
				return last_status;
			break;
		}
	}

	return last_status;
}

pid_t
UnshareForkSpawnInit()
{
	assert(!init_signal_fd.IsDefined());

	if (unshare(CLONE_NEWPID) < 0)
		throw MakeErrno("unshare(CLONE_NEWPID) failed");

	pid_t pid = fork();
	if (pid < 0)
		throw MakeErrno("fork() failed");

	if (pid > 0)
		return pid;

	sigemptyset(&init_signal_mask);
	sigaddset(&init_signal_mask, SIGINT);
	sigaddset(&init_signal_mask, SIGQUIT);
	sigaddset(&init_signal_mask, SIGTERM);
	sigaddset(&init_signal_mask, SIGCHLD);

	sigprocmask(SIG_BLOCK, &init_signal_mask, nullptr);

	SetProcessName("init");

	CloseAllFiles();

	if (!init_signal_fd.CreateSignalFD(&init_signal_mask, false)) {
		perror("signalfd() failed");
		_exit(EXIT_FAILURE);
	}

	_exit(SpawnInit(0, true));
}
