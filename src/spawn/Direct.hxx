// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

#include <utility>

#include <sys/types.h>

struct PreparedChildProcess;
struct CgroupState;
class UniqueFileDescriptor;

struct SpawnChildProcessResult {
	UniqueFileDescriptor pidfd;

	/**
	 * A classic PID (for legacy callers which cannot work with
	 * pidfds).
	 */
	pid_t pid;

	/**
	 * A pipe the holds a lease for resources received from the
	 * spawn-accessory daemon (e.g. namespace handles).
	 */
	UniqueFileDescriptor accessory_lease_pipe;
};

/**
 * Throws exception on error.
 *
 * @param cgroups_group_writable shall cgroups created by this
 * function be writable by the owner gid?
 *
 * @param is_sys_admin are we CAP_SYS_ADMIN?
 */
[[nodiscard]]
SpawnChildProcessResult
SpawnChildProcess(PreparedChildProcess &&params,
		  const CgroupState &cgroup_state,
		  bool cgroups_group_writable,
		  bool is_sys_admin);
