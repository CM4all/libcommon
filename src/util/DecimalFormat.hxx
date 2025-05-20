// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

constexpr char *
format_2digit(char *dest, uint_least16_t number) noexcept
{
	*dest++ = (char)('0' + (number / 10));
	*dest++ = (char)('0' + number % 10);
	return dest;
}

constexpr char *
format_4digit(char *dest, uint_least32_t number) noexcept
{
	*dest++ = (char)('0' + number / 1000);
	*dest++ = (char)('0' + (number / 100) % 10);
	*dest++ = (char)('0' + (number / 10) % 10);
	*dest++ = (char)('0' + number % 10);
	return dest;
}
