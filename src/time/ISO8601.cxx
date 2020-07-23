/*
 * Copyright 2007-2019 Content Management AG
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

#include "ISO8601.hxx"
#include "Convert.hxx"
#include "util/StringBuffer.hxx"

#include <cassert>
#include <cstdlib>
#include <stdexcept>

StringBuffer<64>
FormatISO8601(const struct tm &tm) noexcept
{
	StringBuffer<64> buffer;
	strftime(buffer.data(), buffer.capacity(),
#ifdef _WIN32
		 "%Y-%m-%dT%H:%M:%SZ",
#else
		 "%FT%TZ",
#endif
		 &tm);
	return buffer;
}

StringBuffer<64>
FormatISO8601(std::chrono::system_clock::time_point tp)
{
	return FormatISO8601(GmTime(tp));
}

#ifndef _WIN32

static std::pair<unsigned, unsigned>
ParseTimeZoneOffsetRaw(const char *&s)
{
	char *endptr;
	unsigned long value = std::strtoul(s, &endptr, 10);
	if (endptr == s + 4) {
		s = endptr;
		return std::make_pair(value / 100, value % 100);
	} else if (endptr == s + 2) {
		s = endptr;

		unsigned hours = value, minutes = 0;
		if (*s == ':') {
			++s;
			minutes = std::strtoul(s, &endptr, 10);
			if (endptr != s + 2)
				throw std::runtime_error("Failed to parse time zone offset");

			s = endptr;
		}

		return std::make_pair(hours, minutes);
	} else
		throw std::runtime_error("Failed to parse time zone offset");
}

static std::chrono::system_clock::duration
ParseTimeZoneOffset(const char *&s)
{
	assert(*s == '+' || *s == '-');

	bool negative = *s == '-';
	++s;

	auto raw = ParseTimeZoneOffsetRaw(s);
	if (raw.first > 13)
		throw std::runtime_error("Time offset hours out of range");

	if (raw.second >= 60)
		throw std::runtime_error("Time offset minutes out of range");

	std::chrono::system_clock::duration d = std::chrono::hours(raw.first);
	d += std::chrono::minutes(raw.second);

	if (negative)
		d = -d;

	return d;
}

static const char *
ParseTimeOfDay(const char *s, struct tm &tm,
	       std::chrono::system_clock::duration &precision) noexcept
{
	/* this function always checks "end==s" to work around a
	   strptime() bug on OS X: if nothing could be parsed,
	   strptime() returns the input string (indicating success)
	   instead of nullptr (indicating error) */

	const char *end = strptime(s, "%H", &tm);
	if (end == nullptr || end == s)
		return end;

	s = end;
	precision = std::chrono::hours(1);

	if (*s == ':') {
		/* with field separators: now a minute must follow */

		++s;

		end = strptime(s, "%M", &tm);
		if (end == nullptr || end == s)
			return nullptr;

		s = end;
		precision = std::chrono::minutes(1);

		/* the "seconds" field is optional */
		if (*s != ':')
			return s;

		++s;

		end = strptime(s, "%S", &tm);
		if (end == nullptr || end == s)
			return nullptr;

		precision = std::chrono::seconds(1);
		return end;
	}

	/* without field separators */

	end = strptime(s, "%M", &tm);
	if (end == nullptr || end == s)
		return s;

	s = end;
	precision = std::chrono::minutes(1);

	end = strptime(s, "%S", &tm);
	if (end == nullptr || end == s)
		return s;

	precision = std::chrono::seconds(1);
	return end;
}

#endif

std::pair<std::chrono::system_clock::time_point,
	  std::chrono::system_clock::duration>
ParseISO8601(const char *s)
{
	assert(s != nullptr);

#ifdef _WIN32
	/* TODO: emulate strptime()? */
	(void)s;
	throw std::runtime_error("Time parsing not implemented on Windows");
#else
	struct tm tm{};

	/* parse the date */
	const char *end = strptime(s, "%F", &tm);
	if (end == nullptr) {
		/* try without field separators */
		end = strptime(s, "%Y%m%d", &tm);
		if (end == nullptr)
			throw std::runtime_error("Failed to parse date");
	}

	s = end;

	std::chrono::system_clock::duration precision = std::chrono::hours(24);

	/* parse the time of day */
	if (*s == 'T') {
		++s;

		s = ParseTimeOfDay(s, tm, precision);
		if (s == nullptr)
			throw std::runtime_error("Failed to parse time of day");
	}

	auto tp = TimeGm(tm);

	/* time zone */
	if (*s == 'Z')
		++s;
	else if (*s == '+' || *s == '-')
		tp -= ParseTimeZoneOffset(s);

	if (*s != 0)
		throw std::runtime_error("Garbage at end of time stamp");

	return std::make_pair(tp, precision);
#endif /* !_WIN32 */
}
