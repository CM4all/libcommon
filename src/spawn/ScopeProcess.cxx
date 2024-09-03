// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ScopeProcess.hxx"
#include "spawn/config.h"
#include "system/linux/clone3.h"
#include "system/linux/CloseRange.hxx"
#include "system/Error.hxx"
#include "system/ProcessName.hxx"
#include "io/linux/ProcFdinfo.hxx"
#include "util/Sanitizer.hxx"

#ifdef HAVE_LIBCAP
#include "lib/cap/State.hxx"
#endif

#ifdef HAVE_LIBSECCOMP
#include "SeccompFilter.hxx"
#endif

#include <cstdint> // for uintptr_t

#include <limits.h> // for UINT_MAX
#include <signal.h>
#include <unistd.h> // for _exit()

static void
DropCapabilities()
{
#ifdef HAVE_LIBCAP
	CapabilityState state = CapabilityState::Empty();
	state.Install();
#endif // HAVE_LIBCAP
}

#ifdef HAVE_LIBSECCOMP

/**
 * Install a very strict seccomp filter which allows only very few
 * system calls.
 */
static void
LimitSysCalls()
{
	using Seccomp::Arg;

	Seccomp::Filter sf{SCMP_ACT_KILL};

	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(read), Arg(0) == STDIN_FILENO);
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(exit_group));
	sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(exit));

	/* seccomp_load() may call free(), which may attempt to give
	   heap memory back to the kernel using brk() - this rule
	   ignores this (to prevent SIGKILL) */
	sf.AddRule(SCMP_ACT_ERRNO(ENOMEM), SCMP_SYS(brk));

	if (HaveAddressSanitizer()) {
		/* needed by libasan (if AddressSanitizer is
		   enabled) */
		sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(sigaltstack));
		sf.AddRule(SCMP_ACT_ALLOW, SCMP_SYS(mmap));
	}

	sf.Load();
}

#endif // HAVE_LIBSECCOMP

SystemdScopeProcess
StartSystemdScopeProcess(const bool pid_namespace)
{
	UniqueFileDescriptor pipe_r;
	SystemdScopeProcess p;

	if (!UniqueFileDescriptor::CreatePipe(pipe_r, p.pipe_w))
		throw MakeErrno("pipe() failed");

	int _pidfd;
	const struct clone_args clone_args{
		.flags = CLONE_NEWIPC|CLONE_NEWNET|CLONE_NEWNS|CLONE_NEWUSER|CLONE_PIDFD,
		.pidfd = (uintptr_t)&_pidfd,
	};

	p.local_pid = clone3(&clone_args, sizeof(clone_args));
	if (p.local_pid < 0)
		throw MakeErrno("clone() failed");

	if (p.local_pid == 0) {
		SetProcessName("scope");
		DropCapabilities();

		pipe_r.CheckDuplicate(FileDescriptor{STDIN_FILENO});
		pipe_r.Release();
		p.pipe_w.Release();

		sys_close_range(3, UINT_MAX, 0);

		/* ignore all signals which may stop us; shut down
		   only when the pipe gets closed */
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		signal(SIGUSR1, SIG_IGN);
		signal(SIGUSR2, SIG_IGN);

#ifdef HAVE_LIBSECCOMP
		LimitSysCalls();
#endif

		std::byte dummy;
		[[maybe_unused]]
		auto nbytes = read(STDIN_FILENO, &dummy, sizeof(dummy));

		_exit(EXIT_SUCCESS);
	}

	const UniqueFileDescriptor pidfd{_pidfd};

	/* if we're in a non-root PID namespace, extract the real PID
	   from /proc/self/fdinfo/PIDFD (this is still the old
	   /proc) */
	p.real_pid = pid_namespace
		? ReadPidfdPid(pidfd)
		: p.local_pid;

	return p;
}
