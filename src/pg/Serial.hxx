// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>

namespace Pg {

/**
 * C++ representation of a PostgreSQL "serial" value.
 */
class Serial {
	using value_type = uint_least32_t;
	value_type value;

public:
	Serial() = default;
	explicit constexpr Serial(value_type _value) noexcept:value(_value) {}

	constexpr value_type get() const noexcept {
		return value;
	}

	explicit constexpr operator bool() const noexcept {
		return value != 0;
	}

	/**
	 * Convert a string to a #Serial instance.  Throws
	 * std::invalid_argument on error.
	 */
	[[nodiscard]]
	static Serial Parse(const char *s);
};

/**
 * C++ representation of a PostgreSQL "bigserial" value.
 */
class BigSerial {
	using value_type = uint_least64_t;
	value_type value;

public:
	BigSerial() = default;
	explicit constexpr BigSerial(value_type _value) noexcept:value(_value) {}

	constexpr value_type get() const noexcept {
		return value;
	}

	explicit constexpr operator bool() const noexcept {
		return value != 0;
	}

	/**
	 * Convert a string to a #BigSerial instance.  Throws
	 * std::invalid_argument on error.
	 */
	[[nodiscard]]
	static BigSerial Parse(const char *s);
};

}
