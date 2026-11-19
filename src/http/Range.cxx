// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Range.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

void
HttpRangeRequest::ParseRangeHeader(const char *p) noexcept
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

		uint_least64_t v = strtoull(p, &endptr, 10);
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

			uint_least64_t v = strtoull(p, &endptr, 10);
			if (*endptr != 0 || v < skip) {
				type = Type::INVALID;
				return;
			}

			/* RFC 9110 14.1.2: a last-pos at or past the
			   end of the entity is replaced with the
			   position of its last byte */
			if (v < size)
				size = v + 1;
		}
	}

	type = Type::VALID;
}
