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

#include "Launch.hxx"
#include "Config.hxx"
#include "Systemd.hxx"
#include "CgroupState.hxx"
#include "Server.hxx"
#include "system/CapabilityState.hxx"
#include "system/Error.hxx"
#include "system/ProcessName.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/PrintException.hxx"

#include <sched.h>
#include <fcntl.h> // for AT_SYMLINK_NOFOLLOW
#include <unistd.h>
#include <string.h>
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

#ifdef HAVE_LIBSYSTEMD

static void
Chown(const CgroupState::Mount &cgroup_mount, uid_t uid, gid_t gid) noexcept
{
	fchownat(cgroup_mount.fd.Get(), ".", uid, gid, AT_SYMLINK_NOFOLLOW);
	fchownat(cgroup_mount.fd.Get(), "cgroup.procs", uid, gid,
		 AT_SYMLINK_NOFOLLOW);
}

/**
 * chown() the specified control group and its "cgroup.procs" file in
 * all controllers.
 *
 * This is necessary if we are running in a user namespace, because
 * the Linux kernel requires write permissions to cgroup.procs in the
 * root user namespace ("init_user_ns") for some operations.  Write
 * permissions in the current namespace is not enough.
 *
 * @see
 * https://github.com/torvalds/linux/blob/99613159ad749543621da8238acf1a122880144e/kernel/cgroup/cgroup.c#L4856
 */
static void
Chown(const CgroupState &cgroup_state, uid_t uid, gid_t gid) noexcept
{
	for (const auto &mount : cgroup_state.mounts)
		Chown(mount, uid, gid);
}

#endif // HAVE_LIBSYSTEMD

/**
 * Drop capabilities which are not needed during normal spawner
 * operation.
 */
static void
DropCapabilities()
{
	static constexpr cap_value_t drop_caps[] = {
		/* not needed at all by the spawner */
		CAP_DAC_READ_SEARCH,
		CAP_NET_BIND_SERVICE,

		/* only needed during initialization */
		CAP_CHOWN,
	};

	auto state = CapabilityState::Current();
	state.SetFlag(CAP_EFFECTIVE, drop_caps, CAP_CLEAR);
	state.SetFlag(CAP_PERMITTED, drop_caps, CAP_CLEAR);
	state.Install();
}

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

	try {
		ctx.config.spawner_uid_gid.Apply();
	} catch (...) {
		PrintException(std::current_exception());
		return EXIT_FAILURE;
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

			Chown(cgroup_state, ctx.config.spawner_uid_gid.uid,
			      ctx.config.spawner_uid_gid.gid);
		} catch (...) {
			fprintf(stderr, "Failed to create systemd scope: ");
			PrintException(std::current_exception());
		}
	}
#endif

	try {
		cgroup_state.EnableAllControllers();
	} catch (...) {
		fprintf(stderr, "Failed to setup cgroup2: ");
		PrintException(std::current_exception());
		return EXIT_FAILURE;
	}

	try {
		DropCapabilities();
	} catch (...) {
		fprintf(stderr, "Failed to drop capabilities: ");
		PrintException(std::current_exception());
	}

	try {
		RunSpawnServer(ctx.config, cgroup_state, ctx.hook, std::move(ctx.socket));
		return EXIT_SUCCESS;
	} catch (...) {
		PrintException(std::current_exception());
		return EXIT_FAILURE;
	}
}

UniqueFileDescriptor
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
	int _pidfd;
	int pid = clone(RunSpawnServer2, stack + sizeof(stack),
			CLONE_NEWPID | CLONE_NEWNS | CLONE_PIDFD,
			&ctx, &_pidfd);
	if (pid < 0) {
		/* try again without CLONE_NEWPID */
		fprintf(stderr, "Failed to create spawner PID namespace (%s), trying without\n",
			strerror(errno));
		ctx.pid_namespace = false;
		pid = clone(RunSpawnServer2, stack + sizeof(stack),
			    CLONE_PIDFD,
			    &ctx, &_pidfd);
	}

	/* note: CLONE_IO cannot be used here because it conflicts
	   with the cgroup2 "io" controller which doesn't allow shared
	   IO contexts in different groups; see blkcg_can_attach() in
	   the Linux kernel sources (5.11 as of this writing) */

	if (pid < 0)
		throw MakeErrno("clone() failed");

	UniqueFileDescriptor pidfd{_pidfd};

	/* send its "real" PID to the spawner */
	read_pipe.Close();
	write_pipe.Write(&pid, sizeof(pid));

	return pidfd;
}
