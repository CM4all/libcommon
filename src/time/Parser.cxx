// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Parser.hxx"
#include "Math.hxx"
#include "ISO8601.hxx"
#include "util/StringAPI.hxx"

#include <cstdlib> // for strtoll()
#include <stdexcept>

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
