// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <set>

class FileDescriptor;

/**
 * Write "deny" to /proc/self/setgroups which is necessary for
 * unprivileged processes to set up a gid_map.  See Linux commits
 * 9cc4651 and 66d2f33 for details.
 *
 * Errors are ignored silently.
 */
void
DenySetGroups(FileDescriptor proc_pid) noexcept;

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

[[nodiscard]]
char *
FormatIdMap(char *p, const IdMap::Item item) noexcept;

[[nodiscard]]
static inline char *
FormatIdMap(char *p, unsigned id) noexcept
{
	return FormatIdMap(p, IdMap::Item{.id = id, .mapped_id = id});
}

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
 * @param proc_pid a /proc/PID O_PATH file descriptor
 */
void
SetupUidMap(FileDescriptor proc_pid, const IdMap &map);

void
SetupUidMap(FileDescriptor proc_pid, unsigned uid);

/**
 * Set up a gid mapping for a user namespace.
 *
 * Throws on error.
 *
 * @param proc_pid a /proc/PID O_PATH file descriptor
 * @param gid the group id to be mapped inside the user namespace
 */
void
SetupGidMap(FileDescriptor proc_pid, unsigned gid);

/**
 * Set up a gid mapping for a user namespace.
 *
 * Throws on error.
 *
 * @param proc_pid a /proc/PID O_PATH file descriptor
 * @param gids the group ids to be mapped inside the user namespace
 */
void
SetupGidMap(FileDescriptor proc_pid, const std::set<unsigned> &gids);
