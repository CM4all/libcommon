/*
 * Copyright 2007-2020 CM4all GmbH
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

#include "Timestamp.hxx"
#include "time/Convert.hxx"
#include "util/StringBuffer.hxx"

#include <stdexcept>

#include <assert.h>

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
