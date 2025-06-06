// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Launch.hxx"
#include "ErrorPipe.hxx"
#include "Config.hxx"
#include "ScopeProcess.hxx"
#include "Systemd.hxx"
#include "CgroupState.hxx"
#include "Server.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "spawn/config.h"
#include "system/linux/clone3.h"
#include "system/Error.hxx"
#include "system/Mount.hxx"
#include "system/ProcessName.hxx"
#include "net/EasyMessage.hxx"
#include "net/SocketPair.hxx"
#include "io/Pipe.hxx"
#include "io/Open.hxx"
#include "io/SmallTextFile.hxx"

#ifdef HAVE_LIBCAP
#include "lib/cap/State.hxx"
#endif

#include <fmt/format.h>

#include <concepts>

#include <sched.h>
#include <fcntl.h> // for AT_SYMLINK_NOFOLLOW
#include <unistd.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h> // for AF_LOCAL
#include <sys/stat.h> // for fchmodat()
#include <sys/wait.h>

using std::string_view_literals::operator""sv;

#ifdef HAVE_LIBSYSTEMD

#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

static void
Chown(FileDescriptor mount, uid_t uid, gid_t gid, gid_t procs_gid) noexcept
{
	fchownat(mount.Get(), ".", uid, gid, AT_SYMLINK_NOFOLLOW);
	fchownat(mount.Get(), "cgroup.procs", uid, procs_gid,
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
Chown(const CgroupState &cgroup_state, uid_t uid,
      gid_t gid, gid_t procs_gid) noexcept
{
	Chown(cgroup_state.group_fd, uid, gid, procs_gid);
}

#endif // HAVE_LIBSYSTEMD

static void
SetupPidNamespace()
{
	/* if the spawner runs in its own PID namespace, we need to
	   mount a new /proc for that namespace; first make the
	   existing /proc a "slave" mount (to avoid propagating the
	   new /proc into the parent namespace), and mount the new
	   /proc */
	MountSetAttr(FileDescriptor::Undefined(), "/",
		     AT_RECURSIVE|AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT,
		     0, 0, MS_SLAVE);
	umount2("/proc", MNT_DETACH);

	if (mount("proc", "/proc", "proc", MS_NOEXEC|MS_NOSUID|MS_NODEV, nullptr) < 0)
		throw MakeErrno("Failed to mount new /proc");

	/* mount a new tmpfs on /tmp because some spawner subsystems
	   (e.g. MOUNT_NAMED_TMPFS) might need temporary files, and we
	   want the kernel to clean them up automatically */
	umount2("/tmp", MNT_DETACH);
	if (mount("tmpfs", "/tmp", "tmpfs", MS_NOEXEC|MS_NOSUID|MS_NODEV,
		  "size=16M,nr_inodes=1048576,mode=1777") < 0)
		throw MakeErrno("Failed to mount new /tmp");
}

/**
 * Drop capabilities which are not needed during normal spawner
 * operation.
 */
static void
DropCapabilities()
{
#ifdef HAVE_LIBCAP
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
#endif // HAVE_LIBCAP
}

static int
RunSpawnServer2(const SpawnConfig &config, SpawnHook *hook,
		UniqueSocketDescriptor socket,
		FileDescriptor error_pipe_w,
		const bool pid_namespace) noexcept
{
#ifdef HAVE_LIBSYSTEMD
	SystemdScopeProcess scope_process;

	if (!config.systemd_scope.empty()) {
		try {
			scope_process = StartSystemdScopeProcess(pid_namespace);
		} catch (...) {
			WriteErrorPipe(error_pipe_w,
				       "Failed to start scope process: ",
				       std::current_exception());
			return EXIT_FAILURE;
		}
	}
#endif

	SetProcessName("spawn");

	try {
		if (pid_namespace)
			SetupPidNamespace();

		config.spawner_uid_gid.Apply();
	} catch (...) {
		WriteErrorPipe(error_pipe_w, {}, std::current_exception());
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
						   scope_process.real_pid,
						   scope_process.local_pid,
						   true,
						   config.systemd_slice.empty() ? nullptr : config.systemd_slice.c_str());

			Chown(cgroup_state, config.spawner_uid_gid.effective_uid,
			      config.cgroups_writable_by_gid > 0 ? config.cgroups_writable_by_gid : config.spawner_uid_gid.effective_gid,
			      config.spawner_uid_gid.effective_gid);

			if (config.cgroups_writable_by_gid > 0)
				/* if all cgroups shall be writable by
				   the configured gid, do "chmod g+w"
				   as well as "g+s" (so the owning gid
				   propagates to new cgroups) */
				if (fchmodat(cgroup_state.group_fd.Get(), ".",
					     S_ISGID|0775, AT_SYMLINK_NOFOLLOW) < 0)
					throw MakeErrno("Failed to chmod() the cgroup");
		} catch (...) {
			if (config.systemd_scope_optional) {
				fmt::print(stderr, "Failed to create systemd scope: {}\n",
					   std::current_exception());
			} else {
				WriteErrorPipe(error_pipe_w,
					       "Failed to create systemd scope: ",
					       std::current_exception());
				return EXIT_FAILURE;
			}

			/* stop the "scope" process, we don't need it
			   if we don't have a systemd scope */
			scope_process.pipe_w.Close();

			/* reap the "scope" process; __WCLONE is
			   necessary because the process was cloned
			   without exit_signal=SIGCHLD */
			waitpid(scope_process.local_pid, nullptr, __WCLONE);
		}
	}

	if (cgroup_state.IsEnabled()) {
		try {
			cgroup_state.EnableAllControllers(scope_process.local_pid);
		} catch (...) {
			WriteErrorPipe(error_pipe_w,
				       "Failed to setup cgroup2: ",
				       std::current_exception());
			return EXIT_FAILURE;
		}
	}
#endif // HAVE_LIBSYSTEMD

	try {
		EasySendMessage(socket, cgroup_state.group_fd);
	} catch (...) {
		WriteErrorPipe(error_pipe_w,
			       "Failed to send cgroup fd: ",
			       std::current_exception());
		return EXIT_FAILURE;
	}

	try {
		DropCapabilities();
	} catch (...) {
		fmt::print(stderr, "Failed to drop capabilities: {}\n",
			   std::current_exception());
	}

	error_pipe_w.Close();

	try {
		RunSpawnServer(config, cgroup_state, pid_namespace,
			       hook, std::move(socket));
		return EXIT_SUCCESS;
	} catch (...) {
		fmt::print(stderr, "{}\n", std::current_exception());
		return EXIT_FAILURE;
	}
}

/**
 * Throws on error.
 *
 * @return a pidfd
 */
static UniqueFileDescriptor
LaunchSpawnServer(const SpawnConfig &config, SpawnHook *hook,
		  UniqueSocketDescriptor socket,
		  std::invocable<> auto post_clone)
{
	/**
	 * If an error occurs during setup, the child process will
	 * write an error message to this pipe.
	 */
	auto [error_pipe_r, error_pipe_w] = CreatePipe();

	bool pid_namespace = true;

	int _pidfd;

	struct clone_args ca{
		.flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_PIDFD,
		.pidfd = (uintptr_t)&_pidfd,
		.exit_signal = SIGCHLD,
	};

	/* try to run the spawner in a new PID namespace; to be able to
	   mount a new /proc for this namespace, we need a mount namespace
	   (CLONE_NEWNS) as well */
	int pid = clone3(&ca, sizeof(ca));
	if (pid < 0) {
		/* try again without CLONE_NEWPID */
		fmt::print(stderr, "Failed to create spawner PID namespace ({}), trying without\n",
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

		error_pipe_r.Close();

		_exit(RunSpawnServer2(config, hook, std::move(socket),
				      error_pipe_w,
				      pid_namespace));
	}

	error_pipe_w.Close();

	UniqueFileDescriptor pidfd{AdoptTag{}, _pidfd};

	ReadErrorPipe(error_pipe_r);

	return pidfd;
}

LaunchSpawnServerResult
LaunchSpawnServer(const SpawnConfig &config, SpawnHook *hook)
{
	auto [for_client, for_server] = CreateSocketPairNonBlock(SOCK_SEQPACKET);

	auto pidfd = LaunchSpawnServer(config, hook, std::move(for_server),
				       [&]{ for_client.Close(); });

	(void)for_client.WaitReadable(-1);
	auto cgroup = EasyReceiveMessageWithOneFD(for_client);

	return {
		.pidfd = std::move(pidfd),
		.socket = std::move(for_client),
		.cgroup = std::move(cgroup),
	};
}
