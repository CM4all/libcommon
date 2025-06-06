// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "spawn/config.h"

#include <cstdint>
#include <cstddef>

namespace Spawn {

static constexpr std::size_t MAX_DATAGRAM_SIZE = 32768;

/*
 * This header contains definitions for the internal protocol between
 * #SpawnServerClient and #SpawnServerConnection.  It is not a stable
 * protocol, because both client and server are contained in the same
 * executable (even though the server runs in a forked child process
 * as root).
 */

enum class RequestCommand : uint8_t {
	CONNECT,
	EXEC,
	KILL,
};

enum class ExecCommand : uint8_t {
	EXEC_FUNCTION,
	EXEC_PATH,
	EXEC_FD,
	ARG,
	SETENV,
	UMASK,
	STDIN,
	STDOUT,
	STDOUT_IS_STDIN,
	STDERR,
	STDERR_IS_STDIN,
	STDERR_PATH,
	RETURN_STDERR,
	RETURN_PIDFD,
	RETURN_CGROUP,
	CONTROL,
	TTY,
	USER_NS,
	PID_NS,
	PID_NS_NAME,
	CGROUP_NS,
	NETWORK_NS,
	NETWORK_NS_NAME,
	IPC_NS,
	MOUNT_PROC,
	WRITABLE_PROC,
	MOUNT_DEV,
	MOUNT_PTS,
	BIND_MOUNT_PTS,
	PIVOT_ROOT,
	MOUNT_ROOT_TMPFS,
	MOUNT_TMP_TMPFS,
	MOUNT_TMPFS,
	MOUNT_NAMED_TMPFS,
	BIND_MOUNT,
	BIND_MOUNT_FILE,
	FD_BIND_MOUNT,
	FD_BIND_MOUNT_FILE,
	WRITE_FILE,
	SYMLINK,
	DIR_MODE,
	HOSTNAME,
	RLIMIT,
	UID_GID,
	MAPPED_REAL_UID,
	MAPPED_EFFECTIVE_UID,
#ifdef HAVE_LIBSECCOMP
	FORBID_USER_NS,
	FORBID_MULTICAST,
	FORBID_BIND,
#endif // HAVE_LIBSECCOMP
#ifdef HAVE_LIBCAP
	CAP_SYS_RESOURCE,
#endif // HAVE_LIBCAP
	NO_NEW_PRIVS,
	CGROUP,
	CGROUP_SESSION,
	CGROUP_SET,
	CGROUP_XATTR,
	PRIORITY,
	SCHED_IDLE_,
	IOPRIO_IDLE,
	CHROOT,
	CHDIR,
	HOOK_INFO,
};

enum class ResponseCommand : uint8_t {
	/**
	 * An EXEC request has completed.  This exists to allow the
	 * #SpawnServerClient to count the number of pending requests,
	 * which may then be used to throttle further requests.
	 */
	EXEC_COMPLETE,

	EXIT,
};

struct MemoryWarningPayload {
	uint64_t memory_usage, memory_max;
};

} // namespace Spawn
