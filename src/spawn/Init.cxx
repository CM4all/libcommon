// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Init.hxx"
#include "SeccompFilter.hxx"
#include "lib/cap/State.hxx"
#include "system/CloseRange.hxx"
#include "system/Error.hxx"
#include "system/ProcessName.hxx"
#include "system/LinuxFD.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/PrintException.hxx"

#include <sys/wait.h>
#include <sys/signalfd.h>
#include <sys/capability.h>
#include <unistd.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // for UINT_MAX

static sigset_t init_signal_mask;

pid_t
SpawnInitFork(const char *name)
{
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
		if (name != nullptr) {
			char buffer[16];
			size_t length = strlen(name);

			const char *prefix = length > 10
				? "i" : "init";

			snprintf(buffer, sizeof(buffer), "%s-%s",
				 prefix, name);
			SetProcessName(buffer);
		} else
			SetProcessName("init");

		sys_close_range(3, UINT_MAX, 0);
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
	state.SetFlag(CAP_EFFECTIVE, keep_caps, CAP_SET);
	state.SetFlag(CAP_PERMITTED, keep_caps, CAP_SET);
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

	/* this one is used by ~UniqueFileDescriptor() (created by
	   CreateSignalFD()) */
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(close));

	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(read), Arg(0) == read_fd.Get());
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(wait4));
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(waitid));
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(kill), Arg(0) == kill_pid);
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(exit_group));
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(exit));

	/* seccomp_load() may call free(), which may attempt to give
	   heap memory back to the kernel using brk() - this rule
	   ignores this (to prevent SIGKILL) */
	sf.AddRule(SCMP_ACT_ERRNO(ENOMEM), SCMP_SYS(brk));

	sf.Load();
}

int
SpawnInit(pid_t child_pid, bool remain)
{
	DropCapabilities();

	auto init_signal_fd = CreateSignalFD(init_signal_mask, false);

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
}

pid_t
UnshareForkSpawnInit()
{
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

	sys_close_range(3, UINT_MAX, 0);

	try {
		_exit(SpawnInit(0, true));
	} catch (...) {
		PrintException(std::current_exception());
		_exit(EXIT_FAILURE);
	}
}
