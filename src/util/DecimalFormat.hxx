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
