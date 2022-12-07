/*
 * Copyright 2007-2022 CM4all GmbH
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

	constexpr operator bool() const noexcept {
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

	constexpr operator bool() const noexcept {
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
