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

#include "../../src/pg/Array.hxx"

#include <gtest/gtest.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void
check_decode(const char *input, const char *const* expected)
{
	const auto a = Pg::DecodeArray(input);

	unsigned i = 0;
	for (const auto &v : a) {
		if (expected[i] == NULL) {
			fprintf(stderr, "decode '%s': too many elements in result ('%s')\n",
				input, v.c_str());
			FAIL();
		}

		if (strcmp(v.c_str(), expected[i]) != 0) {
			fprintf(stderr, "decode '%s': element %u differs: '%s', but '%s' expected\n",
				input, i, v.c_str(), expected[i]);
			FAIL();
		}

		++i;
	}

	if (expected[i] != NULL) {
		fprintf(stderr, "decode '%s': not enough elements in result ('%s')\n",
			input, expected[i]);
		FAIL();
	}
}

TEST(PgTest, DecodeArray)
{
	const char *zero[] = {NULL};
	const char *empty[] = {"", NULL};
	const char *one[] = {"foo", NULL};
	const char *two[] = {"foo", "bar", NULL};
	const char *three[] = {"foo", "", "bar", NULL};
	const char *special[] = {"foo", "\"\\", NULL};

	check_decode("{}", zero);
	check_decode("{\"\"}", empty);
	check_decode("{foo}", one);
	check_decode("{\"foo\"}", one);
	check_decode("{foo,bar}", two);
	check_decode("{foo,\"bar\"}", two);
	check_decode("{foo,,bar}", three);
	check_decode("{foo,\"\\\"\\\\\"}", special);
}
