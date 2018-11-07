/*
 * Copyright 2007-2017 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EXPIRY_HXX
#define EXPIRY_HXX

#include <chrono>

/**
 * Helper library for handling expiry time stamps using the system's
 * monotonic clock.
 */
class Expiry {
	using clock_type = std::chrono::steady_clock;
	using value_type = clock_type::time_point;
	using duration_type = clock_type::duration;

	value_type value;

	constexpr Expiry(value_type _value) noexcept:value(_value) {}

public:
	Expiry() = default;

	static Expiry Now() noexcept {
		return clock_type::now();
	}

	static constexpr Expiry AlreadyExpired() noexcept {
		return value_type::min();
	}

	static constexpr Expiry Never() noexcept {
		return value_type::max();
	}

	static constexpr Expiry Touched(Expiry now,
					duration_type duration) noexcept {
		return now.value + duration;
	}

	static Expiry Touched(duration_type duration) noexcept {
		return Touched(Now(), duration);
	}

	void Touch(Expiry now, duration_type duration) noexcept {
		value = now.value + duration;
	}

	void Touch(duration_type duration) noexcept {
		Touch(Now(), duration);
	}

	constexpr bool IsExpired(Expiry now) const noexcept {
		return now >= *this;
	}

	bool IsExpired() const noexcept {
		return IsExpired(Now());
	}

	constexpr bool operator==(Expiry other) const noexcept {
		return value == other.value;
	}

	constexpr bool operator<(Expiry other) const noexcept {
		return value < other.value;
	}

	constexpr bool operator<=(Expiry other) const noexcept {
		return value <= other.value;
	}

	constexpr bool operator>(Expiry other) const noexcept {
		return value > other.value;
	}

	constexpr bool operator>=(Expiry other) const noexcept {
		return value >= other.value;
	}
};

#endif
