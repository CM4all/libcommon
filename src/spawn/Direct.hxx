// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <utility>

#include <sys/types.h>

struct PreparedChildProcess;
struct CgroupState;
class UniqueFileDescriptor;

/**
 * Throws exception on error.
 *
 * @param cgroups_group_writable shall cgroups created by this
 * function be writable by the owner gid?
 *
 * @param is_sys_admin are we CAP_SYS_ADMIN?
 *
 * @return a pidfd and a classic pid (the latter for legacy callers
 * which cannot work with pidfds)
 */
[[nodiscard]]
std::pair<UniqueFileDescriptor, pid_t>
SpawnChildProcess(PreparedChildProcess &&params,
		  const CgroupState &cgroup_state,
		  bool cgroups_group_writable,
		  bool is_sys_admin);
