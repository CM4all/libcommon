// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <type_traits>

constexpr char base32_digits[] = "0123456789abcdefghijklmnopqrstuv";

static_assert(sizeof(base32_digits) == 33);

/**
 * Convert an integer to base32.  Both the alphabet and the order is
 * arbitrary, optimized to be fast, but reproducible.
 *
 * The buffer must be large enough to hold the formatted string.  This
 * function will not null-terminate it.
 *
 * @return a pointer to one after the last character
 */
template<typename I>
requires std::is_integral_v<I>
constexpr char *
FormatIntBase32(char *buffer, I _value) noexcept
{
	using U = std::make_unsigned_t<I>;
	U value(_value);

	do {
		*buffer++ = base32_digits[value & 0x1f];
		value >>= 5;
	} while (value);

	return buffer;
}
