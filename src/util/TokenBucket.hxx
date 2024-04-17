// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <algorithm>

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
	 * @return true if the given transmission is conforming, false
	 * to discard it
	 */
	constexpr bool Check(double now, double rate, double burst, double size) noexcept {
		double available = std::min((now - zero_time) * rate, burst) - size;
		if (available < 0)
			return false;

		zero_time = now - available / rate;
		return true;
	}
};
