// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Timestamp.hxx"
#include "time/Convert.hxx"
#include "util/StringBuffer.hxx"

#include <cassert>
#include <stdexcept>

namespace Pg {

static std::chrono::system_clock::duration
ParsePositiveTimezoneOffset(const char *s)
{
	char *endptr;
	const auto hours = strtoul(s, &endptr, 10);
	if (endptr != s + 2 || hours >= 24)
		throw std::runtime_error("Failed to parse time zone offset");

	s = endptr;

	std::chrono::system_clock::duration result = std::chrono::hours(hours);

	if (*s == ':') {
		++s;
		const auto minutes = strtoul(s, &endptr, 10);
		if (endptr != s + 2 || hours >= 60)
			throw std::runtime_error("Failed to parse time zone offset");

		s = endptr;

		result += std::chrono::minutes(minutes);
	}

	return result;
}

std::chrono::system_clock::time_point
ParseTimestamp(const char *s)
{
	assert(s != nullptr);

	struct tm tm;
	const char *end = strptime(s, "%F %T", &tm);
	if (end == nullptr)
		throw std::runtime_error("Failed to parse PostgreSQL timestamp");

	s = end;

	auto t = TimeGm(tm);

	if (*s == '.') {
		/* parse fractional part */
		char *endptr;
		double fractional_s = strtod(s, &endptr);
		if (endptr == s)
			throw std::runtime_error("Failed to parse PostgreSQL timestamp");

		assert(fractional_s >= 0);
		assert(fractional_s < 1);

		s = endptr;

		const std::chrono::duration<double> f(fractional_s);
		t += std::chrono::duration_cast<std::chrono::system_clock::duration>(f);
	}

	switch (*s) {
	case '+':
		t -= ParsePositiveTimezoneOffset(s + 1);
		break;

	case '-':
		t += ParsePositiveTimezoneOffset(s + 1);
		break;
	}

	return t;
}

static StringBuffer<64>
FormatTimestamp(const struct tm &tm) noexcept
{
	StringBuffer<64> buffer;
	strftime(buffer.data(), buffer.capacity(), "%F %T", &tm);
	return buffer;
}

StringBuffer<64>
FormatTimestamp(std::chrono::system_clock::time_point tp) noexcept
{
	return FormatTimestamp(GmTime(tp));
}

}
