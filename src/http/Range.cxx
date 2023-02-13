// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
