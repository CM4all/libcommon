// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <chrono>

/**
 * Configuration for #ExponentialBackoff.  To avoid bloating the
 * #ExponentialBackoff class, this must be called to each method call.
 */
struct ExponentialBackoffConfig {
	std::chrono::steady_clock::duration min_delay;
	std::chrono::steady_clock::duration max_delay;
};

/**
 * Simple implementation of the Exponential Backoff algorithm: when a
 * system fails, retry after a certain delay, and let this delay grow
 * exponentially.
 */
class ExponentialBackoff {
	/**
	 * Do not retry until this time point.
	 */
	std::chrono::steady_clock::time_point until;

	/**
	 * The delay for the next failure.
	 */
	std::chrono::steady_clock::duration delay;

public:
	constexpr ExponentialBackoff(const ExponentialBackoffConfig config) noexcept
		:delay(config.min_delay) {}

	constexpr void Reset(const ExponentialBackoffConfig config) noexcept {
		until = {};
		delay = config.min_delay;
	}

	/**
	 * @return true if the operation can be attempted again, false
	 * if it is still blocked
	 */
	constexpr bool Check(std::chrono::steady_clock::time_point now) noexcept {
		return now >= until;
	}

	/**
	 * Block the operation with a delay.
	 */
	constexpr void Delay(const ExponentialBackoffConfig config,
			     std::chrono::steady_clock::time_point now) noexcept {
		until = now + delay;
		delay = std::min(delay * 2, config.max_delay);
	}
};
