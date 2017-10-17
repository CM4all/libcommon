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

#include "Range.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

void
HttpRangeRequest::ParseRangeHeader(const char *p)
{
	assert(p != nullptr);
	assert(type == Type::NONE);
	assert(skip == 0);

	if (strncmp(p, "bytes=", 6) != 0) {
		type = Type::INVALID;
		return;
	}

	p += 6;

	char *endptr;

	if (*p == '-') {
		/* suffix-byte-range-spec */
		++p;

		uint64_t v = strtoull(p, &endptr, 10);
		if (v >= size)
			return;

		skip = size - v;
	} else {
		skip = strtoull(p, &endptr, 10);
		if (skip >= size) {
			type = Type::INVALID;
			return;
		}

		if (*endptr == '-') {
			p = endptr + 1;
			if (*p == 0) {
				/* "wget -c" */
				type = Type::VALID;
				return;
			}

			uint64_t v = strtoull(p, &endptr, 10);
			if (*endptr != 0 || v < skip || v >= size) {
				type = Type::INVALID;
				return;
			}

			size = v + 1;
		}
	}

	type = Type::VALID;
}
