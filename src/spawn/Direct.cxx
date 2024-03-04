// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Direct.hxx"
#include "ErrorPipe.hxx"
#include "Prepared.hxx"
#include "CgroupOptions.hxx"
#include "Init.hxx"
#include "spawn/config.h"
#include "accessory/Client.hxx"
#include "lib/fmt/SystemError.hxx"
#include "net/EasyMessage.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Pipe.hxx"
#include "io/ScopeUmask.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/WriteFile.hxx"
#include "io/linux/UserNamespace.hxx"
#include "system/clone3.h"
#include "system/CoreScheduling.hxx"
#include "system/IOPrio.hxx"
#include "util/Exception.hxx"
#include "util/ScopeExit.hxx"

#ifdef HAVE_LIBSECCOMP
#include "SeccompFilter.hxx"
#include "SyscallFilter.hxx"
#endif

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-journal.h>
#endif

#include <stdexcept>

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sched.h>
#include <poll.h> // for POLLIN

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

static void
CheckedDup2(FileDescriptor oldfd, FileDescriptor newfd) noexcept
{
	if (oldfd.IsDefined())
		oldfd.CheckDuplicate(newfd);
}

static void
CheckedDup2(FileDescriptor oldfd, int newfd) noexcept
{
	CheckedDup2(oldfd, FileDescriptor(newfd));
}

static void
DisconnectTty() noexcept
{
	FileDescriptor fd;
	if (fd.Open("/dev/tty", O_RDWR)) {
		(void) ioctl(fd.Get(), TIOCNOTTY, nullptr);
		fd.Close();
	}
}

static void
UnignoreSignals() noexcept
{
	/* restore all signals which were set to SIG_IGN by
	   RunSpawnServer() and others */
	static constexpr int signals[] = {
		SIGHUP,
		SIGINT, SIGQUIT,
		SIGPIPE,
		SIGTERM,
		SIGUSR1, SIGUSR2,
		SIGCHLD,
		SIGTRAP,
	};

	for (auto i : signals)
		signal(i, SIG_DFL);
}

static void
UnblockSignals() noexcept
{
	sigset_t mask;
	sigfillset(&mask);
	sigprocmask(SIG_UNBLOCK, &mask, nullptr);
}

/**
 * Send one byte to the pipe and close it.  This wakes up the peer
 * which blocks inside WaitForPipe().
 */
static void
WakeUpPipe(UniqueFileDescriptor &&w) noexcept
{
	assert(w.IsDefined());

	static constexpr std::byte one_byte[1]{};
	(void)w.Write(one_byte);
	w.Close();
}

/**
 * Read one byte from the pipe.
 *
 * @return true if there was exactly one byte in the pipe
 */
static bool
WaitForPipe(FileDescriptor r) noexcept
{
	assert(r.IsDefined());

	/* expect one byte to indicate success, and then the pipe will
	   be closed by the parent */
	std::byte buffer[1];

	return r.Read(buffer) == sizeof(buffer) &&
		r.Read(buffer) == 0;
}

[[noreturn]]
static void
Exec(const char *path, PreparedChildProcess &&p,
     const bool skip_uid_gid,
     const char *name,
     UniqueFileDescriptor &&userns_map_pipe_r,
     UniqueFileDescriptor &&userns_create_pipe_w,
     UniqueFileDescriptor &&wait_pipe_r,
     UniqueFileDescriptor &&error_pipe_w) noexcept
try {
	assert(error_pipe_w.IsDefined());

	UnignoreSignals();
	UnblockSignals();

	if (p.umask >= 0)
		umask(p.umask);

	TryWriteExistingFile("/proc/self/oom_score_adj",
			     p.ns.mount.pivot_root == nullptr
			     ? "700"
			     /* higher OOM score adjustment for jailed
				(per-account?) processes */
			     : "800");

	FileDescriptor stdout_fd = p.stdout_fd, stderr_fd = p.stderr_fd;
#ifdef HAVE_LIBSYSTEMD
	if (!stdout_fd.IsDefined() ||
	    (!stderr_fd.IsDefined() && p.stderr_path == nullptr)) {
		/* if no log destination was specified, log to the systemd
		   journal */
		/* note: this must be done before NamespaceOptions::Apply(),
		   because inside the new root, we don't have access to
		   /run/systemd/journal/stdout */
		int journal_fd = sd_journal_stream_fd(p.args.front(), LOG_INFO, true);
		if (!stdout_fd.IsDefined())
			stdout_fd = FileDescriptor{journal_fd};
		if (!stderr_fd.IsDefined() && p.stderr_path == nullptr)
			stderr_fd = FileDescriptor{journal_fd};
	}
#endif

	if (userns_map_pipe_r.IsDefined() && !WaitForPipe(userns_map_pipe_r))
		_exit(EXIT_FAILURE);

	/* switch to the final UID/GID before setting up the mount
	   namespace, because the previous UID/GID may not be mapped
	   in the user namespace, causing mkdir() to fail with
	   EOVERFLOW */
	/* note: we need to do this only if we're already in a new
	   user namespace; if CLONE_NEWUSER was postponed, the
	   EOVERFLOW problem is not relevant, and switching UID/GID
	   early would require the spawner to have CAP_SYS_RESOURCE
	   for prlimit() */
	const bool early_uid_gid = !wait_pipe_r.IsDefined();
	if (early_uid_gid && !skip_uid_gid)
		p.uid_gid.Apply();

	p.ns.Apply(p.uid_gid);

	if (!wait_pipe_r.IsDefined())
		/* if the wait_pipe exists, then the parent process
		   will apply the resource limits */
		p.rlimits.Apply(0);

	if (p.chroot != nullptr && chroot(p.chroot) < 0)
		throw FmtErrno("chroot('{}') failed", p.chroot);

	if (userns_create_pipe_w.IsDefined()) {
		/* user namespace allocation was postponed to allow
		   mounting /proc with a reassociated PID namespace
		   (which would not be allowed from inside a new user
		   namespace, because the user namespace drops
		   capabilities on the PID namespace) */

		assert(wait_pipe_r.IsDefined());

		if (unshare(CLONE_NEWUSER) < 0)
			throw MakeErrno("unshare(CLONE_NEWUSER) failed");

		/* after success (no exception was thrown), wake up
		   the parent */
		WakeUpPipe(std::move(userns_create_pipe_w));
	}

	/* wait for the parent to set us up */
	if (wait_pipe_r.IsDefined() && !WaitForPipe(wait_pipe_r))
		_exit(EXIT_FAILURE);

	if (p.sched_idle) {
		static struct sched_param sched_param;
		sched_setscheduler(0, SCHED_IDLE, &sched_param);
	}

	if (p.priority != 0 &&
	    setpriority(PRIO_PROCESS, getpid(), p.priority) < 0)
		throw MakeErrno("setpriority() failed");

	if (p.ioprio_idle)
		ioprio_set_idle();

	if (p.tty)
		DisconnectTty();

	if (p.ns.enable_pid && p.ns.pid_namespace == nullptr) {
		setsid();

		const auto pid = SpawnInitFork(name);
		assert(pid >= 0);

		if (pid > 0)
			_exit(SpawnInit(pid, false));
	}

	/* if this is a jailed process, we assume it's unprivileged
	   and should not share a HT core with a process for a
	   different user to avoid cross-HT attacks, so create a new
	   core scheduling cookie */
	/* failure to do so will be ignored silently, because the
	   Linux kernel may not have that feature yet */
	if (p.ns.mount.pivot_root != nullptr)
		CoreScheduling::Create(0);

	if (p.no_new_privs)
		prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

#ifdef HAVE_LIBSECCOMP
	try {
		Seccomp::Filter sf(SCMP_ACT_ALLOW);

#ifdef PR_SET_NO_NEW_PRIVS
		/* don't enable PR_SET_NO_NEW_PRIVS unless the feature
		   was explicitly enabled */
		if (!p.no_new_privs)
			sf.SetAttributeNoThrow(SCMP_FLTATR_CTL_NNP, 0);
#endif

		sf.AddSecondaryArchs();

		BuildSyscallFilter(sf);

		if (p.forbid_user_ns)
			ForbidUserNamespace(sf);

		if (p.forbid_multicast)
			ForbidMulticast(sf);

		if (p.forbid_bind)
			ForbidBind(sf);

		sf.Load();
	} catch (const std::runtime_error &e) {
		if (p.HasSyscallFilter())
			/* filter options have been explicitly enabled, and thus
			   failure to set up the filter are fatal */
			throw;

		fprintf(stderr, "Failed to setup seccomp filter for '%s': %s\n",
			path, e.what());
	}
#endif // HAVE_LIBSECCOMP

	if (!early_uid_gid && !skip_uid_gid) {
		if (p.ns.mapped_uid > 0 && p.ns.mapped_uid != p.uid_gid.uid) {
			/* we need to use the mapped_uid because the
			   original uid isn't valid from inside this
			   user namespace */
			auto mapped = p.uid_gid;
			mapped.uid = p.ns.mapped_uid;
			mapped.Apply();
		} else
			p.uid_gid.Apply();
	}

	if (p.chdir != nullptr && chdir(p.chdir) < 0)
		throw FmtErrno("chdir('{}') failed", p.chdir);

	if (!stderr_fd.IsDefined() && p.stderr_path != nullptr &&
	    !stderr_fd.Open(p.stderr_path,
			    O_CREAT|O_WRONLY|O_APPEND,
			    0600))
		throw MakeErrno("Failed to open STDERR_PATH");

	if (p.return_stderr.IsDefined()) {
		assert(stderr_fd.IsDefined());

		EasySendMessage(p.return_stderr, stderr_fd);
		p.return_stderr.Close();
	}

	constexpr int CONTROL_FILENO = 3;
	CheckedDup2(p.stdin_fd, STDIN_FILENO);
	CheckedDup2(stdout_fd, STDOUT_FILENO);
	CheckedDup2(stderr_fd, STDERR_FILENO);
	CheckedDup2(p.control_fd, CONTROL_FILENO);

	if (p.session)
		setsid();

	if (p.tty) {
		assert(p.stdin_fd.IsDefined());
		assert(p.stdin_fd == p.stdout_fd);

		if (ioctl(p.stdin_fd.Get(), TIOCSCTTY, nullptr) < 0)
			throw MakeErrno("Failed to set the controlling terminal");
	}

	if (p.exec_function != nullptr) {
		_exit(p.exec_function(std::move(p)));
	} else {
		execve(path, const_cast<char *const*>(p.args.data()),
		       const_cast<char *const*>(p.env.data()));

		throw FmtErrno("Failed to execute '{}'", path);
	}
} catch (...) {
	WriteErrorPipe(error_pipe_w, {}, std::current_exception());
	_exit(EXIT_FAILURE);
}

/**
 * Read an error message from the pipe and if there is one, throw it
 * as a C++ exception.
 */
static void
ReadErrorPipeTimeout(FileDescriptor error_pipe_r)
{
	if (int p = error_pipe_r.WaitReadable(250);
	    p <= 0 || (p & POLLIN) == 0)
		/* this can time out if the execve() takes a long time
		   to finish (maybe because the shrinker runs) and the
		   other side of the pipe doesn't get closed early
		   enough through O_CLOEXEC */
		// TODO find a better solution
		return;

	ReadErrorPipe(error_pipe_r);
}

std::pair<UniqueFileDescriptor, pid_t>
SpawnChildProcess(PreparedChildProcess &&params,
		  const CgroupState &cgroup_state,
		  bool cgroups_group_writable,
		  bool is_sys_admin)
{
	uint_least64_t clone_flags = CLONE_CLEAR_SIGHAND|CLONE_PIDFD;
	clone_flags = params.ns.GetCloneFlags(clone_flags);

	const char *path = params.Finish();

	/**
	 * If an error occurs during setup, the child process will
	 * write an error message to this pipe.
	 */
	auto [error_pipe_r, error_pipe_w] = CreatePipe();

	UniqueFileDescriptor old_pidns;

	AtScopeExit(&old_pidns) {
		/* restore the old namespaces */
		if (old_pidns.IsDefined())
			setns(old_pidns.Get(), CLONE_NEWPID);
	};

	if (params.ns.pid_namespace != nullptr) {
		/* first open a handle to our existing (old) namespaces
		   to be able to restore them later (see above) */
		if (!old_pidns.OpenReadOnly("/proc/self/ns/pid"))
			throw MakeErrno("Failed to open current PID namespace");

		auto fd = SpawnAccessory::MakePidNamespace(SpawnAccessory::Connect(),
							   params.ns.pid_namespace);
		if (setns(fd.Get(), CLONE_NEWPID) < 0)
			throw MakeErrno("setns(CLONE_NEWPID) failed");
	}

	/**
	 * A pipe used by the parent process to wait for the child to
	 * create the user namespace.
	 */
	UniqueFileDescriptor userns_create_pipe_r, userns_create_pipe_w;

	/**
	 * The child waits for this pipe before it applies namespaces.
	 */
	UniqueFileDescriptor userns_map_pipe_r, userns_map_pipe_w;

	/**
	 * A pipe used by the child process to wait for the parent to set
	 * it up (e.g. uid/gid mappings).
	 */
	UniqueFileDescriptor wait_pipe_r, wait_pipe_w;

	/**
	 * In "debug mode", uid/gid setup is skipped (because the
	 * application is unprivileged and cannot switch uid/gid).
	 * UidGid::IsNop() must be checked from outside the new user
	 * namespace or else getresuid()/getresgid() will only return
	 * the "overflow" ids, and UidGid::IsNop() always returns
	 * false.
	 */
	const bool skip_uid_gid = params.uid_gid.IsNop();

	if (params.ns.enable_user && is_sys_admin) {
		/* from inside the new user namespace, we cannot
		   reassociate with a new network namespace or mount
		   /proc of a reassociated PID namespace, because at
		   this point, we have lost capabilities on those
		   namespaces; therefore, postpone CLONE_NEWUSER until
		   everything is set up; to synchronize this, create
		   two pairs of pipes */

		if (!UniqueFileDescriptor::CreatePipe(userns_create_pipe_r,
						      userns_create_pipe_w))
			throw MakeErrno("pipe() failed");

		if (!UniqueFileDescriptor::CreatePipe(wait_pipe_r,
						      wait_pipe_w))
			throw MakeErrno("pipe() failed");

		/* disable CLONE_NEWUSER for the clone() call, because
		   the child process will call
		   unshare(CLONE_NEWUSER) */
		clone_flags &= ~CLONE_NEWUSER;

		/* this process will set up the uid/gid maps, so
		   disable that part in the child process */
		params.ns.enable_user = false;
	} else if (params.ns.enable_user && !skip_uid_gid) {
		/* if we have to set a user or group without being
		   CAP_SYS_ADMIN (only CAP_SETUID/CAP_SETGID,
		   e.g. inside a container), then the child process
		   doesn't have those capabilities; we need to set up
		   uid/gid mappings before it sets up its mount
		   namespace, or else creating mount points with
		   mkdir() in tmpfs fails with EOVERFLOW */
		/* note that this EOVERFLOW does not occur with
		   CAP_SYS_ADMIN, because CAP_SYS_ADMIN allows us to
		   clone() without CLONE_NEWUSER and unshare() it
		   later, i.e. the new user namespace does not yet
		   exist the child calls mkdir() */

		if (!UniqueFileDescriptor::CreatePipe(userns_map_pipe_r,
						      userns_map_pipe_w))
			throw MakeErrno("pipe() failed");

		/* this process will set up the uid/gid maps, so
		   disable that part in the child process */
		params.ns.enable_user = false;
	}

	int _pidfd;

	struct clone_args ca{
		.flags = clone_flags,
		.pidfd = (uintptr_t)&_pidfd,
		.exit_signal = SIGCHLD,
	};

	/* if a cgroup name is specified, it is used as the name for
	   the "init" process */
	const char *const name = params.cgroup != nullptr
		? params.cgroup->name
		: nullptr;

	UniqueFileDescriptor cgroup_fd;
	if (params.cgroup != nullptr) {
		const ScopeUmask scope_umask{
			cgroups_group_writable ? mode_t{0002} : mode_t{0022}
		};

		cgroup_fd = params.cgroup->Create2(cgroup_state,
						   params.cgroup_session);
		if (cgroup_fd.IsDefined()) {
			ca.flags |= CLONE_INTO_CGROUP;
			ca.cgroup = cgroup_fd.Get();

			if (params.return_cgroup.IsDefined())
				EasySendMessage(params.return_cgroup,
						cgroup_fd);
		}
	}

	if (params.return_cgroup.IsDefined())
		params.return_cgroup.Close();

	long pid = clone3(&ca, sizeof(ca));
	if (pid < 0)
		throw MakeErrno("clone() failed");

	if (pid == 0) {
		userns_map_pipe_w.Close();
		userns_create_pipe_r.Close();
		wait_pipe_w.Close();
		error_pipe_r.Close();

		Exec(path, std::move(params),
		     skip_uid_gid,
		     name,
		     std::move(userns_map_pipe_r),
		     std::move(userns_create_pipe_w),
		     std::move(wait_pipe_r),
		     std::move(error_pipe_w));
	}

	error_pipe_w.Close();

	UniqueFileDescriptor pidfd{_pidfd};

	if (userns_map_pipe_w.IsDefined()) {
		/* set up the child's uid/gid mapping and wake it
		   up */

		if (is_sys_admin)
			/* do this only if we have CAP_SYS_ADMIN
			   (i.e. not already in a container); without
			   it, we can't call setgroups() in the new
			   child process because of this
			   self-inflicted restriction */
			DenySetGroups(pid);

		params.ns.SetupUidGidMap(params.uid_gid, pid);

		/* now the right process is ready to set up its mount
		   namespace */
		WakeUpPipe(std::move(userns_map_pipe_w));
	}

	if (userns_create_pipe_r.IsDefined()) {
		/* wait for the child to create the user namespace */

		userns_create_pipe_w.Close();

		/* expect one byte to indicate success, and then the
		   pipe will be closed by the child */
		if (!WaitForPipe(userns_create_pipe_r)) {
			/* read the error_pipe, it may have more
			   details */
			ReadErrorPipeTimeout(error_pipe_r);
			throw std::runtime_error("User namespace setup failed");
		}
	}

	if (wait_pipe_w.IsDefined()) {
		/* set up the child's uid/gid mapping and wake it up */
		wait_pipe_r.Close();
		params.ns.SetupUidGidMap(params.uid_gid, pid);

		/* apply the resource limits in the parent process, because
		   the child has lost all root namespace capabilities by
		   entering a new user namespace */
		params.rlimits.Apply(pid);

		/* after success (no exception was thrown), wake up
		   the child */
		WakeUpPipe(std::move(wait_pipe_w));
	}

	ReadErrorPipeTimeout(error_pipe_r);

	if (params.return_pidfd.IsDefined()) {
		EasySendMessage(params.return_pidfd, pidfd);
		params.return_pidfd.Close();
	}

	/* TODO don't return the "classic" pid_t as soon as all
	   callers have been fully migrated to pidfd */
	return {std::move(pidfd), pid};
}
