// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

inline void
format_2digit(char *dest, uint_least16_t number) noexcept
{
	dest[0] = (char)('0' + (number / 10));
	dest[1] = (char)('0' + number % 10);
}

inline  void
format_4digit(char *dest, uint_least32_t number) noexcept
{
	dest[0] = (char)('0' + number / 1000);
	dest[1] = (char)('0' + (number / 100) % 10);
	dest[2] = (char)('0' + (number / 10) % 10);
	dest[3] = (char)('0' + number % 10);
}

/**
 * Format a 64 bit unsigned integer into a decimal string.
 */
inline std::size_t
format_uint64(char dest[32], uint_least64_t number) noexcept
{
	char *p = dest + 32 - 1;

	*p = 0;
	do {
		--p;
		*p = '0' + (char)(number % 10);
		number /= 10;
	} while (number != 0);

	if (p > dest)
		memmove(dest, p, dest + 32 - p);

	return dest + 32 - p - 1;
}
