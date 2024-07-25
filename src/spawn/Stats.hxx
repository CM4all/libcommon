// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

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
	 * How many child processse are alive right now?
	 */
	std::size_t alive;
};
