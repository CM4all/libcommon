// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>

class FileDescriptor;

/**
 * Parsed contents of a cgroup "memory.events" file.
 */
struct CgroupMemoryEvents {
	uint_least32_t oom_kill;
};

/**
 * Read and parse the contents of a "memory.events" file.
 *
 * Throws on read error.
 *
 * @params fd a file descriptor for a readable "memory.events" file
 */
CgroupMemoryEvents
ReadCgroupMemoryEvents(FileDescriptor fd);

/**
 * Parsed contents of a cgroup "pids.events" file.
 */
struct CgroupPidsEvents {
	uint_least32_t max;
};

/**
 * Read and parse the contents of a "pids.events" file.
 *
 * Throws on read error.
 *
 * @params fd a file descriptor for a readable "pids.events" file
 */
CgroupPidsEvents
ReadCgroupPidsEvents(FileDescriptor fd);
