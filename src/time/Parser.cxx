/*
 * Copyright 2019-2022 CM4all GmbH
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

#include "Parser.hxx"
#include "Math.hxx"
#include "ISO8601.hxx"
#include "util/StringAPI.hxx"

#include <cstdlib> // for strtoll()

static std::pair<std::chrono::system_clock::duration,
		 std::chrono::system_clock::duration>
ParseDuration(const char *s)
{
	char *endptr;
	auto i = strtoll(s, &endptr, 10);
	if (endptr == s)
		throw std::invalid_argument{"Failed to parse number"};

	std::chrono::system_clock::duration resolution;
	switch (*endptr) {
	case 's':
		resolution = std::chrono::seconds{1};
		break;

	case 'm':
		resolution = std::chrono::minutes{1};
		break;

	case 'h':
		resolution = std::chrono::hours{1};
		break;

	case 'd':
		resolution = std::chrono::hours{24};
		break;

	default:
		throw std::invalid_argument{"Invalid unit"};
	}

	if (endptr[1] != 0)
		throw std::invalid_argument{"Invalid unit"};

	return {
		i * resolution,
		resolution,
	};
}

std::pair<std::chrono::system_clock::time_point,
	  std::chrono::system_clock::duration>
ParseTimePoint(const char *s)
{
	if (StringIsEqual(s, "today"))
		return {
			PrecedingMidnightLocal(std::chrono::system_clock::now()),
			std::chrono::hours(24),
		};

	if (StringIsEqual(s, "yesterday"))
		return {
			PrecedingMidnightLocal(std::chrono::system_clock::now()) - std::chrono::hours{24},
			std::chrono::hours(24),
		};

	if (StringIsEqual(s, "tomorrow"))
		return {
			PrecedingMidnightLocal(std::chrono::system_clock::now()) + std::chrono::hours{24},
			std::chrono::hours(24),
		};

	if (StringIsEqual(s, "now"))
		return {
			std::chrono::system_clock::now(),
			std::chrono::system_clock::duration::zero(),
		};

	if (*s == '-' || *s == '+') {
		/* duration relative to now */
		auto [duration, resolution] = ParseDuration(s);

		return {
			std::chrono::system_clock::now() + duration,
			resolution,
		};
	}

	return ParseISO8601(s);
}
