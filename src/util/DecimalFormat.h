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

#ifndef DECIMAL_FORMAT_H
#define DECIMAL_FORMAT_H

#include "Compiler.h"

#include <stdint.h>
#include <string.h>

static gcc_always_inline void
format_2digit(char *dest, unsigned number)
{
	dest[0] = (char)('0' + (number / 10));
	dest[1] = (char)('0' + number % 10);
}

static gcc_always_inline void
format_4digit(char *dest, unsigned number)
{
	dest[0] = (char)('0' + number / 1000);
	dest[1] = (char)('0' + (number / 100) % 10);
	dest[2] = (char)('0' + (number / 10) % 10);
	dest[3] = (char)('0' + number % 10);
}

/**
 * Format a 64 bit unsigned integer into a decimal string.
 */
static gcc_always_inline size_t
format_uint64(char dest[32], uint64_t number)
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

#endif
