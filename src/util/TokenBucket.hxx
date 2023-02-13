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
	void Reset() noexcept {
		zero_time = 0;
	}

	/**
	 * @return true if the given transmission is conforming, false
	 * to discard it
	 */
	bool Check(double now, double rate, double burst, double size) noexcept {
		double available = std::min((now - zero_time) * rate, burst);
		if (available < size)
			return false;

		zero_time = now - (available - size) / rate;
		return true;
	}
};
