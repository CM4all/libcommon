// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

public:
	Expiry() = default;

	constexpr Expiry(value_type _value) noexcept:value(_value) {}

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

	constexpr duration_type GetRemainingDuration(Expiry now) const noexcept {
		return value - now.value;
	}

	constexpr bool operator==(Expiry other) const noexcept {
		return value == other.value;
	}

	constexpr bool operator!=(Expiry other) const noexcept {
		return value != other.value;
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
