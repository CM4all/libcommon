// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <set>

/**
 * Write "deny" to /proc/self/setgroups which is necessary for
 * unprivileged processes to set up a gid_map.  See Linux commits
 * 9cc4651 and 66d2f33 for details.
 */
void
DenySetGroups();

/**
 * Set up a uid mapping for a user namespace.
 *
 * @param pid the process id whose user namespace shall be modified; 0
 * for current process
 * @param uid the user id to be mapped inside the user namespace
 * @param root true to also map root
 */
void
SetupUidMap(unsigned pid, unsigned uid, bool root);

/**
 * Set up a gid mapping for a user namespace.
 *
 * @param pid the process id whose user namespace shall be modified; 0
 * for current process
 * @param gid the group id to be mapped inside the user namespace
 * @param root true to also map root
 */
void
SetupGidMap(unsigned pid, unsigned gid, bool root);

/**
 * Set up a gid mapping for a user namespace.
 *
 * @param pid the process id whose user namespace shall be modified; 0
 * for current process
 * @param gids the group ids to be mapped inside the user namespace
 */
void
SetupGidMap(unsigned pid, const std::set<unsigned> &gids);
