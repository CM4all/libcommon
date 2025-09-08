// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <set>

/**
 * Write "deny" to /proc/self/setgroups which is necessary for
 * unprivileged processes to set up a gid_map.  See Linux commits
 * 9cc4651 and 66d2f33 for details.
 *
 * Errors are ignored silently.
 */
void
DenySetGroups(unsigned pid) noexcept;

/**
 * Uid/gid mapping for Linux user namespaces.
 */
struct IdMap {
	struct Item {
		/** the id to be mapped inside the user namespace */
		unsigned id;

		/** the id the #id parameter is mapped to */
		unsigned mapped_id;
	};

	Item first{}, second{};

	/*** true to also map root */
	bool root = false;
};

/**
 * Format the uid/gid map to a string buffer.  This prepares for
 * writing to `/proc/PID/[ug]id_map`.
 *
 * @param dest the destination buffer
 * @return the end of the destination buffer (not null-terminated)
 */
[[nodiscard]]
char *
FormatIdMap(char *dest, const IdMap &map) noexcept;

[[nodiscard]]
char *
FormatIdMap(char *dest, const std::set<unsigned> &ids) noexcept;

/**
 * Set up a uid mapping for a user namespace.
 *
 * Throws on error.
 *
 * @param pid the process id whose user namespace shall be modified; 0
 * for current process
 */
void
SetupUidMap(unsigned pid, const IdMap &map);

void
SetupUidMap(unsigned pid, unsigned uid);

/**
 * Set up a gid mapping for a user namespace.
 *
 * Throws on error.
 *
 * @param pid the process id whose user namespace shall be modified; 0
 * for current process
 * @param gid the group id to be mapped inside the user namespace
 */
void
SetupGidMap(unsigned pid, unsigned gid);

/**
 * Set up a gid mapping for a user namespace.
 *
 * Throws on error.
 *
 * @param pid the process id whose user namespace shall be modified; 0
 * for current process
 * @param gids the group ids to be mapped inside the user namespace
 */
void
SetupGidMap(unsigned pid, const std::set<unsigned> &gids);
