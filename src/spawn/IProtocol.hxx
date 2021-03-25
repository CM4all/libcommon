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

#pragma once

#include <stdint.h>

/*
 * This header contains definitions for the internal protocol between
 * #SpawnServerClient and #SpawnServerConnection.  It is not a stable
 * protocol, because both client and server are contained in the same
 * executable (even though the server runs in a forked child process
 * as root).
 */

enum class SpawnRequestCommand : uint8_t {
	CONNECT,
	EXEC,
	KILL,
};

enum class SpawnExecCommand : uint8_t {
	ARG,
	SETENV,
	UMASK,
	STDIN,
	STDOUT,
	STDERR,
	STDERR_PATH,
	RETURN_STDERR,
	CONTROL,
	TTY,
	USER_NS,
	PID_NS,
	NETWORK_NS,
	NETWORK_NS_NAME,
	IPC_NS,
	MOUNT_PROC,
	WRITABLE_PROC,
	MOUNT_PTS,
	BIND_MOUNT_PTS,
	PIVOT_ROOT,
	MOUNT_ROOT_TMPFS,
	MOUNT_HOME,
	MOUNT_TMP_TMPFS,
	MOUNT_TMPFS,
	BIND_MOUNT,
	HOSTNAME,
	RLIMIT,
	UID_GID,
	FORBID_USER_NS,
	FORBID_MULTICAST,
	FORBID_BIND,
	NO_NEW_PRIVS,
	CGROUP,
	CGROUP_SESSION,
	CGROUP_SET,
	PRIORITY,
	SCHED_IDLE_,
	IOPRIO_IDLE,
	CHROOT,
	CHDIR,
	HOOK_INFO,
};

enum class SpawnResponseCommand : uint16_t {
	/**
	 * Indicates that cgroups are available.
	 */
	CGROUPS_AVAILABLE,

	/**
	 * Memory usage is above the threshold.  Payload is a
	 * #SpawnMemoryWarningPayload.
	 */
	MEMORY_WARNING,

	EXIT,
};

struct SpawnMemoryWarningPayload {
	uint64_t memory_usage, memory_max;
};
