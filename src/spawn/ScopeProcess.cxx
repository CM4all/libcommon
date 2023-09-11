// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ScopeProcess.hxx"
#include "net/SocketDescriptor.hxx"
#include "system/clone3.h"
#include "system/Error.hxx"
#include "system/ProcessName.hxx"
#include "io/linux/ProcFdinfo.hxx"

#include <cstdint> // for uintptr_t

#include <signal.h>
#include <unistd.h> // for _exit()

SystemdScopeProcess
StartSystemdScopeProcess(SocketDescriptor socket,
			 FileDescriptor error_pipe_w,
			 const bool pid_namespace)
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

		/* ignore all signals which may stop us; shut down
		   only when the pipe gets closed */
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		signal(SIGUSR1, SIG_IGN);
		signal(SIGUSR2, SIG_IGN);

		p.pipe_w.Close();
		error_pipe_w.Close();
		socket.Close();

		// TODO set up seccomp filter

		std::byte dummy;
		pipe_r.Read(&dummy, sizeof(dummy));
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
