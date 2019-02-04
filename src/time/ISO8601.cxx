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

#include "ISO8601.hxx"
#include "Convert.hxx"

#include <stdexcept>

#include <time.h>

std::string
FormatISO8601(const struct tm &tm)
{
	char buffer[64];
	strftime(buffer, sizeof(buffer), "%FT%TZ", &tm);
	return buffer;
}

std::string
FormatISO8601(std::chrono::system_clock::time_point tp)
{
	return FormatISO8601(GmTime(tp));
}

std::pair<std::chrono::system_clock::time_point,
	  std::chrono::system_clock::duration>
ParseISO8601(const char *s)
{
	struct tm tm{};

	/* parse the date */
	const char *end = strptime(s, "%F", &tm);
	if (end == nullptr)
		throw std::runtime_error("Failed to parse date");

	s = end;

	std::chrono::system_clock::duration precision = std::chrono::hours(24);

	/* parse the time of day */
	if (*s == 'T') {
		++s;
		end = strptime(s, "%T", &tm);
		if (end == nullptr)
			throw std::runtime_error("Failed to parse time of day");

		s = end;
		precision = std::chrono::seconds(1);
	}

	if (*s == 'Z')
		++s;

	if (*s != 0)
		throw std::runtime_error("Garbage at end of time stamp");

	return std::make_pair(TimeGm(tm), precision);
}
