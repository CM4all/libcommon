// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <chrono>
#include <cstdint>

struct SpawnStats {
	/**
	 * The total number of SpawnChildProcess() calls (include
         * failed ones).
	 */
	uint_least64_t spawned;

	/**
	 * The number of failed SpawnChildProcess() calls.
	 */
	uint_least64_t errors;

	/**
	 * The number of signals that were sent to processes (via
         * ChildProcessHandle::Kill()).
	 */
	uint_least64_t killed;

	/**
	 * The number of processes that have exited.
	 */
	uint_least64_t exited;

	/**
	 * How many child processse are alive right now?
	 */
	std::size_t alive;

	/**
	 * The current number of pending (incomplete) spawn calls.
	 */
	std::size_t pending;

	/**
	 * The total duration of all SpawnChildProcess() calls.
	 */
	std::chrono::steady_clock::duration total_spawn_duration;
};
