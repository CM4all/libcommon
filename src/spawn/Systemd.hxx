// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <stdint.h>

struct CgroupState;

struct SystemdUnitProperties {
	/**
	 * CPUWeight; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t cpu_weight = 0;

	/**
	 * TasksMax; 0 means "undefined" (i.e. use systemd's default).
	 */
	uint64_t tasks_max = 0;

	/**
	 * MemoryMin; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_min = 0;

	/**
	 * MemoryLow; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_low = 0;

	/**
	 * MemoryHigh; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_high = 0;

	/**
	 * MemoryMax; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_max = 0;

	/**
	 * MemorySwapMax; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_swap_max = 0;

	/**
	 * IOWeight; 0 means "undefined" (i.e. use systemd's default).
	 */
	uint64_t io_weight = 0;
};

/**
 * Create a new systemd scope and move the current process into it.
 *
 * Throws std::runtime_error on error.
 *
 * @param properties properties to be passed to systemd
 * @param slice create the new scope in this slice (optional)
 */
CgroupState
CreateSystemdScope(const char *name, const char *description,
		   const SystemdUnitProperties &properties,
		   int pid, bool delegate=false, const char *slice=nullptr);
