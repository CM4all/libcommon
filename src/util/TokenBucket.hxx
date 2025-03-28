// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <algorithm> // for std::min()

/**
 * An implementation of the "token bucket" rate limiter algorithm.
 *
 * @see https://en.wikipedia.org/wiki/Token_bucket
 */
class TokenBucket {
	double zero_time = 0;

public:
	constexpr void Reset() noexcept {
		zero_time = 0;
	}

	/**
	 * Has the rate limiter reached the zero?
	 */
	constexpr bool IsZero(double now) const noexcept {
		return now >= zero_time;
	}

	/**
	 * Calculate how many tokens are available.
	 */
	constexpr double GetAvailable(double now, double rate, double burst) const noexcept {
		return std::min((now - zero_time) * rate, burst);
	}

	/**
	 * @return true if the given transmission is conforming, false
	 * to discard it
	 */
	constexpr bool Check(double now, double rate, double burst, double size) noexcept {
		double available = GetAvailable(now, rate, burst) - size;
		if (available < 0)
			return false;

		zero_time = now - available / rate;
		return true;
	}

	/**
	 * A non-standard method which always updates, even if the check fails.
	 *
	 * @return the numer of tokens that are available after the
	 * update
	 */
	constexpr double Update(double now, double rate, double burst, double size) noexcept {
		double available = GetAvailable(now, rate, burst) - size;
		zero_time = now - available / rate;
		return available;
	}
};
