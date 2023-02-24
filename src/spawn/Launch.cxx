// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Launch.hxx"
#include "Config.hxx"
#include "Systemd.hxx"
#include "CgroupState.hxx"
#include "Server.hxx"
#include "lib/cap/State.hxx"
#include "system/clone3.h"
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
#include <sys/socket.h> // for AF_LOCAL
#include <sys/mount.h>

#ifdef HAVE_LIBSYSTEMD

#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

static void
Chown(FileDescriptor mount, uid_t uid, gid_t gid) noexcept
{
	fchownat(mount.Get(), ".", uid, gid, AT_SYMLINK_NOFOLLOW);
	fchownat(mount.Get(), "cgroup.procs", uid, gid,
		 AT_SYMLINK_NOFOLLOW);
}

#if __GNUC__
#pragma GCC diagnostic pop
#endif

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
	Chown(cgroup_state.group_fd, uid, gid);
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

	/* don't inherit any of the remaining capabilities to spawned
	   processes */
	state.ClearFlag(CAP_INHERITABLE);

	state.Install();
}

static int
RunSpawnServer2(const SpawnConfig &config, SpawnHook *hook,
		UniqueSocketDescriptor socket,
		FileDescriptor read_pipe,
		const bool pid_namespace) noexcept
{
	/* receive our "real" PID from the parent process; we have no way
	   to obtain it, because we're in a PID namespace and getpid()
	   returns 1 */
	int real_pid;
	if (read_pipe.Read(&real_pid, sizeof(real_pid)) != sizeof(real_pid))
		real_pid = getpid();
	read_pipe.Close();

	SetProcessName("spawn");

	if (pid_namespace) {
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
		config.spawner_uid_gid.Apply();
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
	if (!config.systemd_scope.empty()) {
		try {
			cgroup_state =
				CreateSystemdScope(config.systemd_scope.c_str(),
						   config.systemd_scope_description.c_str(),
						   config.systemd_scope_properties,
						   real_pid, true,
						   config.systemd_slice.empty() ? nullptr : config.systemd_slice.c_str());

			Chown(cgroup_state, config.spawner_uid_gid.uid,
			      config.spawner_uid_gid.gid);
		} catch (...) {
			fprintf(stderr, "Failed to create systemd scope: ");
			PrintException(std::current_exception());

			if (!config.systemd_scope_optional)
				return EXIT_FAILURE;
		}
	}
#endif

	if (cgroup_state.IsEnabled()) {
		try {
			cgroup_state.EnableAllControllers();
		} catch (...) {
			fprintf(stderr, "Failed to setup cgroup2: ");
			PrintException(std::current_exception());
			return EXIT_FAILURE;
		}
	}

	try {
		DropCapabilities();
	} catch (...) {
		fprintf(stderr, "Failed to drop capabilities: ");
		PrintException(std::current_exception());
	}

	try {
		RunSpawnServer(config, cgroup_state, hook, std::move(socket));
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
	/**
	 * A pipe which is used to copy the "real" PID to the spawner
	 * (which doesn't know its own PID because it lives in a new PID
	 * namespace).  The "real" PID is necessary because we need to
	 * send it to systemd.
	 */
	UniqueFileDescriptor read_pipe, write_pipe;
	if (!UniqueFileDescriptor::CreatePipe(read_pipe, write_pipe))
		throw MakeErrno("pipe() failed");

	bool pid_namespace = true;

	int _pidfd;

	struct clone_args ca{};
	ca.flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_PIDFD;
	ca.pidfd = (intptr_t)&_pidfd;
	ca.exit_signal = SIGCHLD;

	/* try to run the spawner in a new PID namespace; to be able to
	   mount a new /proc for this namespace, we need a mount namespace
	   (CLONE_NEWNS) as well */
	int pid = clone3(&ca, sizeof(ca));
	if (pid < 0) {
		/* try again without CLONE_NEWPID */
		fprintf(stderr, "Failed to create spawner PID namespace (%s), trying without\n",
			strerror(-pid));
		pid_namespace = false;

		ca.flags &= ~(CLONE_NEWPID | CLONE_NEWNS);
		pid = clone3(&ca, sizeof(ca));
	}

	/* note: CLONE_IO cannot be used here because it conflicts
	   with the cgroup2 "io" controller which doesn't allow shared
	   IO contexts in different groups; see blkcg_can_attach() in
	   the Linux kernel sources (5.11 as of this writing) */

	if (pid < 0)
		throw MakeErrno(-pid, "clone() failed");

	if (pid == 0) {
		post_clone();

		write_pipe.Close();

		_exit(RunSpawnServer2(config, hook, std::move(socket),
				      read_pipe, pid_namespace));
	}

	UniqueFileDescriptor pidfd{_pidfd};

	/* send its "real" PID to the spawner */
	read_pipe.Close();
	write_pipe.Write(&pid, sizeof(pid));

	return pidfd;
}

UniqueSocketDescriptor
LaunchSpawnServer(const SpawnConfig &config, SpawnHook *hook)
{
	UniqueSocketDescriptor s1, s2;
	if (!UniqueSocketDescriptor::CreateSocketPairNonBlock(AF_LOCAL, SOCK_SEQPACKET, 0,
							      s1, s2))
		throw MakeErrno("socketpair() failed");

	LaunchSpawnServer(config, hook, std::move(s1),
			  [&s2](){ s2.Close(); });

	return s2;
}
