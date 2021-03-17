/*
 * Copyright 2007-2021 CM4all GmbH
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

#ifndef HEX_FORMAT_H
#define HEX_FORMAT_H

#include "Compiler.h"

#include <stdint.h>
#include <string.h>

extern const char hex_digits[];

static gcc_always_inline void
format_uint8_hex_fixed(char dest[2], uint8_t number) noexcept
{
	dest[0] = hex_digits[(number >> 4) & 0xf];
	dest[1] = hex_digits[number & 0xf];
}

static gcc_always_inline void
format_uint16_hex_fixed(char dest[4], uint16_t number) noexcept
{
	dest[0] = hex_digits[(number >> 12) & 0xf];
	dest[1] = hex_digits[(number >> 8) & 0xf];
	dest[2] = hex_digits[(number >> 4) & 0xf];
	dest[3] = hex_digits[number & 0xf];
}

static gcc_always_inline void
format_uint32_hex_fixed(char dest[8], uint32_t number) noexcept
{
	dest[0] = hex_digits[(number >> 28) & 0xf];
	dest[1] = hex_digits[(number >> 24) & 0xf];
	dest[2] = hex_digits[(number >> 20) & 0xf];
	dest[3] = hex_digits[(number >> 16) & 0xf];
	dest[4] = hex_digits[(number >> 12) & 0xf];
	dest[5] = hex_digits[(number >> 8) & 0xf];
	dest[6] = hex_digits[(number >> 4) & 0xf];
	dest[7] = hex_digits[number & 0xf];
}

static gcc_always_inline void
format_uint64_hex_fixed(char dest[16], uint64_t number) noexcept
{
	format_uint32_hex_fixed(dest, number >> 32);
	format_uint32_hex_fixed(dest + 8, number);
}

/**
 * Format a 32 bit unsigned integer into a hex string.
 */
static gcc_always_inline size_t
format_uint32_hex(char dest[9], uint32_t number) noexcept
{
	char *p = dest + 9 - 1;

	*p = 0;
	do {
		--p;
		*p = hex_digits[number % 0x10];
		number /= 0x10;
	} while (number != 0);

	if (p > dest)
		memmove(dest, p, dest + 9 - p);

	return dest + 9 - p - 1;
}

#endif
