// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Launch.hxx"
#include "ErrorPipe.hxx"
#include "Config.hxx"
#include "Systemd.hxx"
#include "CgroupState.hxx"
#include "Server.hxx"
#include "lib/cap/State.hxx"
#include "system/clone3.h"
#include "system/Error.hxx"
#include "system/Mount.hxx"
#include "system/ProcessName.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Open.hxx"
#include "io/SmallTextFile.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/NumberParser.hxx"
#include "util/PrintException.hxx"

#include <sched.h>
#include <fcntl.h> // for AT_SYMLINK_NOFOLLOW
#include <unistd.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h> // for AF_LOCAL

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
		FileDescriptor error_pipe_w,
		const bool pid_namespace) noexcept
{
	/* open the old /proc/self to be able to extract PIDs from the
	   top-level PID namespace which we need for the
	   StartTransientUnit D-Bus request */
	auto old_proc_self = OpenPath("/proc/self", O_DIRECTORY);

	int real_pid;
	if (pid_namespace) {
		/* if we're in a new PID namespace, getpid() will
		   return 1, but for D-Bus, we need our top-level PID
		   which we can read from the old /proc/self/stat */
		try {
			real_pid = WithSmallTextFile<64>(FileAt{old_proc_self, "stat"}, [](std::string_view contents){
				auto p = ParseInteger<int>(Split(contents, ' ').first);
				if (!p)
					throw std::runtime_error{"Failed to parse pid"};
				return *p;
			});
		} catch (...) {
			WriteErrorPipe(error_pipe_w,
				       "Failed to determine real PID: ",
				       std::current_exception());
			return EXIT_FAILURE;
		}
	} else
		real_pid = getpid();

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
						   real_pid, true,
						   config.systemd_slice.empty() ? nullptr : config.systemd_slice.c_str());

			Chown(cgroup_state, config.spawner_uid_gid.uid,
			      config.spawner_uid_gid.gid);
		} catch (...) {
			if (config.systemd_scope_optional) {
				fprintf(stderr, "Failed to create systemd scope: ");
				PrintException(std::current_exception());
			} else {
				WriteErrorPipe(error_pipe_w,
					       "Failed to create systemd scope: ",
					       std::current_exception());
				return EXIT_FAILURE;
			}
		}
	}
#endif

	if (cgroup_state.IsEnabled()) {
		try {
			cgroup_state.EnableAllControllers();
		} catch (...) {
			WriteErrorPipe(error_pipe_w,
				       "Failed to setup cgroup2: ",
				       std::current_exception());
			return EXIT_FAILURE;
		}
	}

	try {
		DropCapabilities();
	} catch (...) {
		fprintf(stderr, "Failed to drop capabilities: ");
		PrintException(std::current_exception());
	}

	error_pipe_w.Close();
	old_proc_self.Close();

	try {
		RunSpawnServer(config, cgroup_state, pid_namespace,
			       hook, std::move(socket));
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
	 * If an error occurs during setup, the child process will
	 * write an error message to this pipe.
	 */
	UniqueFileDescriptor error_pipe_r, error_pipe_w;
	if (!UniqueFileDescriptor::CreatePipe(error_pipe_r, error_pipe_w))
		throw MakeErrno("pipe() failed");

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

		error_pipe_r.Close();

		_exit(RunSpawnServer2(config, hook, std::move(socket),
				      error_pipe_w,
				      pid_namespace));
	}

	error_pipe_w.Close();

	UniqueFileDescriptor pidfd{_pidfd};

	ReadErrorPipe(error_pipe_r);

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
