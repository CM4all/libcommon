// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef PREPARED_CHILD_PROCESS_HXX
#define PREPARED_CHILD_PROCESS_HXX

#include "ResourceLimits.hxx"
#include "NamespaceOptions.hxx"
#include "UidGid.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <forward_list>

struct CgroupOptions;
class UniqueFileDescriptor;
class UniqueSocketDescriptor;

struct PreparedChildProcess {
	/**
	 * An opaque string which may be used by SpawnHook methods.  For
	 * example, it may be a template name.
	 */
	const char *hook_info = nullptr;

	/**
	 * A function pointer which will be called instead of executing a
	 * new program with execve().
	 *
	 * @return the process exit status
	 */
	int (*exec_function)(PreparedChildProcess &&p) = nullptr;

	/**
	 * This program will be executed (unless #exec_function is set).
	 * If this is nullptr, then args.front() will be used.
	 */
	const char *exec_path = nullptr;

	/**
	 * An absolute path where STDERR output will be appended.  This
	 * file will be opened after jailing and after applying the
	 * #UidGid.
	 */
	const char *stderr_path = nullptr;

	std::vector<const char *> args;
	std::vector<const char *> env;
	FileDescriptor stdin_fd = FileDescriptor::Undefined();
	FileDescriptor stdout_fd = FileDescriptor::Undefined();
	FileDescriptor stderr_fd = FileDescriptor::Undefined();
	UniqueFileDescriptor control_fd;

	/**
	 * If defined, then this is a socket where the child process
	 * shall send the newly opened stderr file descriptor.
	 */
	UniqueSocketDescriptor return_stderr;

	/**
	 * If defined, then this is a socket where the spawner shall
	 * send a pidfd.
	 */
	UniqueSocketDescriptor return_pidfd;

	/**
	 * If defined, then this is a socket where the spawner shall
	 * send a cgroup O_PATH file descriptor.  This is useful for
	 * watching resource usage.
	 */
	UniqueSocketDescriptor return_cgroup;

	/**
	 * The umask for the new child process.  -1 means do not change
	 * it.
	 */
	int umask = -1;

	/**
	 * The CPU scheduler priority configured with setpriority(),
	 * ranging from -20 to 19.
	 */
	int priority = 0;

	const CgroupOptions *cgroup = nullptr;

	NamespaceOptions ns;

	ResourceLimits rlimits;

	UidGid uid_gid;

	/**
	 * Change to this new root directory.  This feature should not be
	 * used; use NamespaceOptions::pivot_root instead.  It is only
	 * here for compatibility.
	 */
	const char *chroot = nullptr;

	/**
	 * Change the working directory.
	 */
	const char *chdir = nullptr;

	/**
	 * Select the "idle" CPU scheduling policy.  With this policy, the
	 * "priority" value is ignored.
	 *
	 * @see sched(7)
	 */
	bool sched_idle = false;

	/**
	 * Select the "idle" I/O scheduling class.
	 *
	 * @see ioprio_set(2)
	 */
	bool ioprio_idle = false;

	bool forbid_user_ns = false;

	bool forbid_multicast = false;

	bool forbid_bind = false;

	bool no_new_privs = false;

	/**
	 * Make #stdin_fd and #stdout_fd (which must be equal) the
	 * controlling TTY?
	 */
	bool tty = false;

	/**
	 * Run the process in a new session using setsid()?
	 */
	bool session = true;

	/**
	 * String allocations for SetEnv().
	 */
	std::forward_list<std::string> strings;

	PreparedChildProcess() noexcept;
	~PreparedChildProcess() noexcept;

	PreparedChildProcess(const PreparedChildProcess &) = delete;
	PreparedChildProcess &operator=(const PreparedChildProcess &) = delete;

	/**
	 * Is at least one system call filter option enabled?  If yes,
	 * then failures to set up the filter are fatal.
	 */
	bool HasSyscallFilter() const noexcept {
		return forbid_user_ns || forbid_multicast || forbid_bind;
	}

	void InsertWrapper(std::span<const char *const> w) noexcept;

	void Append(const char *arg) noexcept {
		args.push_back(arg);
	}

	void PutEnv(const char *p) noexcept {
		env.push_back(p);
	}

	void SetEnv(const char *name, const char *value) noexcept;

	const char *GetEnv(std::string_view name) const noexcept;

	void SetStdin(int fd) noexcept;
	void SetStdout(int fd) noexcept;
	void SetStderr(int fd) noexcept;

	void SetStdin(UniqueFileDescriptor fd) noexcept;
	void SetStdout(UniqueFileDescriptor fd) noexcept;
	void SetStderr(UniqueFileDescriptor fd) noexcept;
	void SetControl(UniqueFileDescriptor fd) noexcept {
		control_fd = std::move(fd);
	}

	void SetStdin(UniqueSocketDescriptor fd) noexcept;
	void SetStdout(UniqueSocketDescriptor fd) noexcept;
	void SetStderr(UniqueSocketDescriptor fd) noexcept;
	void SetControl(UniqueSocketDescriptor fd) noexcept;

	/**
	 * Finish this object and return the executable path.
	 */
	const char *Finish() noexcept;
};

#endif
