/*
 * Copyright 2007-2019 Content Management AG
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

#include "Launch.hxx"
#include "Config.hxx"
#include "Systemd.hxx"
#include "CgroupState.hxx"
#include "Server.hxx"
#include "system/Error.hxx"
#include "system/ProcessName.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/PrintException.hxx"

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/mount.h>

struct LaunchSpawnServerContext {
	const SpawnConfig &config;

	SpawnHook *hook;

	UniqueSocketDescriptor socket;

	std::function<void()> post_clone;

	/**
	 * A pipe which is used to copy the "real" PID to the spawner
	 * (which doesn't know its own PID because it lives in a new PID
	 * namespace).  The "real" PID is necessary because we need to
	 * send it to systemd.
	 */
	FileDescriptor read_pipe, write_pipe;

	bool pid_namespace;
};

static int
RunSpawnServer2(void *p)
{
	auto &ctx = *(LaunchSpawnServerContext *)p;

	ctx.post_clone();

	ctx.write_pipe.Close();

	/* receive our "real" PID from the parent process; we have no way
	   to obtain it, because we're in a PID namespace and getpid()
	   returns 1 */
	int real_pid;
	if (ctx.read_pipe.Read(&real_pid, sizeof(real_pid)) != sizeof(real_pid))
		real_pid = getpid();
	ctx.read_pipe.Close();

	SetProcessName("spawn");

	if (ctx.pid_namespace) {
		/* if the spawner runs in its own PID namespace, we need to
		   mount a new /proc for that namespace; first make the
		   existing /proc a "slave" mount (to avoid propagating the
		   new /proc into the parent namespace), and mount the new
		   /proc */
		mount(nullptr, "/", nullptr, MS_SLAVE|MS_REC, nullptr);
		umount2("/proc", MNT_DETACH);
		mount("proc", "/proc", "proc", MS_NOEXEC|MS_NOSUID|MS_NODEV, nullptr);
	}

	/* ignore all signals which may stop us; shut down only when all
	   sockets are closed */
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

	CgroupState cgroup_state;

#ifdef HAVE_LIBSYSTEMD
	if (!ctx.config.systemd_scope.empty()) {
		try {
			cgroup_state =
				CreateSystemdScope(ctx.config.systemd_scope.c_str(),
						   ctx.config.systemd_scope_description.c_str(),
						   ctx.config.systemd_scope_properties,
						   real_pid, true,
						   ctx.config.systemd_slice.empty() ? nullptr : ctx.config.systemd_slice.c_str());
		} catch (const std::runtime_error &e) {
			fprintf(stderr, "Failed to create systemd scope: ");
			PrintException(e);
		}
	}
#endif

	RunSpawnServer(ctx.config, cgroup_state, ctx.hook, std::move(ctx.socket));
	return 0;
}

pid_t
LaunchSpawnServer(const SpawnConfig &config, SpawnHook *hook,
		  UniqueSocketDescriptor socket,
		  std::function<void()> post_clone)
{
	UniqueFileDescriptor read_pipe, write_pipe;
	if (!UniqueFileDescriptor::CreatePipe(read_pipe, write_pipe))
		throw MakeErrno("pipe() failed");

	LaunchSpawnServerContext ctx{config, hook, std::move(socket),
			std::move(post_clone),
			read_pipe, write_pipe, true};

	char stack[32768];

	/* try to run the spawner in a new PID namespace; to be able to
	   mount a new /proc for this namespace, we need a mount namespace
	   (CLONE_NEWNS) as well */
	int pid = clone(RunSpawnServer2, stack + sizeof(stack),
			CLONE_NEWPID | CLONE_NEWNS | CLONE_IO | SIGCHLD,
			&ctx);
	if (pid < 0) {
		/* try again without CLONE_NEWPID */
		fprintf(stderr, "Failed to create spawner PID namespace (%s), trying without\n",
			strerror(errno));
		ctx.pid_namespace = false;
		pid = clone(RunSpawnServer2, stack + sizeof(stack),
			    CLONE_IO | SIGCHLD,
			    &ctx);
	}

	if (pid < 0)
		throw MakeErrno("clone() failed");

	/* send its "real" PID to the spawner */
	read_pipe.Close();
	write_pipe.Write(&pid, sizeof(pid));

	return pid;
}
