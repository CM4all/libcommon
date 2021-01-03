/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "Interval.hxx"
#include "util/StringCompare.hxx"
#include "util/StringStrip.hxx"

#include <cassert>
#include <cstdlib>
#include <stdexcept>

static constexpr std::chrono::hours pg_day(24);

/**
 * PostgreSQL assumes a month has 30 days: "SELECT EXTRACT(EPOCH FROM
 * '1month'::interval)" returns 2592000.
 */
static constexpr auto pg_month = 30 * pg_day;

/**
 * PostgreSQL assumes a year has 365.25 days: "SELECT EXTRACT(EPOCH
 * FROM '1y'::interval)" returns 31557600.
 */
static constexpr auto pg_year = 365 * pg_day + pg_day / 4;

static constexpr struct {
	const char *name;
	std::chrono::seconds value;
} pg_interval_units[] = {
	{ "years", pg_year },
	{ "year", pg_year },
	{ "y", pg_year },
	{ "months", pg_month },
	{ "month", pg_month },
	{ "mons", pg_month },
	{ "mon", pg_month },
	{ "days", pg_day },
	{ "day", pg_day },
	{ "d", pg_day },
	{ "hours", std::chrono::hours(1) },
	{ "hour", std::chrono::hours(1) },
	{ "minutes", std::chrono::minutes(1) },
	{ "minute", std::chrono::minutes(1) },
	{ "min", std::chrono::minutes(1) },
	{ "m", std::chrono::minutes(1) },
	{ "seconds", std::chrono::seconds(1) },
	{ "second", std::chrono::seconds(1) },
	{ "sec", std::chrono::seconds(1) },
	{ "s", std::chrono::seconds(1) },
};

static std::chrono::seconds
ParseTimeOfDay(long hours, const char *s)
{
	if (hours < -24 || hours > 24)
		throw std::invalid_argument("Invalid hour");

	std::chrono::seconds result = std::chrono::hours(labs(hours));

	char *endptr;
	const unsigned long minutes = std::strtoul(s, &endptr, 10);
	if (endptr != s + 2 || *endptr != ':' || minutes >= 60)
		throw std::invalid_argument("Invalid minute");

	result += std::chrono::minutes(minutes);

	s = endptr + 1;
	const unsigned long seconds = std::strtoul(s, &endptr, 10);
	if (endptr != s + 2 || seconds >= 60)
		throw std::invalid_argument("Invalid second");

	result += std::chrono::seconds(seconds);

	s = StripLeft(endptr);
	if (*s != 0)
		throw std::invalid_argument("Garbage after time of day");

	if (hours < 0)
		result = -result;

	return result;
}

std::chrono::seconds
Pg::ParseIntervalS(const char *s)
{
	assert(s != nullptr);

	std::chrono::seconds value(0);

	while (*s != 0) {
		char *endptr;
		long l = std::strtol(s, &endptr, 10);
		if (endptr == s)
			throw std::invalid_argument("Failed to parse number");

		if (*endptr == ':') {
			value += ParseTimeOfDay(l, endptr + 1);
			break;
		}

		s = StripLeft(endptr);

		for (const auto &i : pg_interval_units) {
			if (auto end = StringAfterPrefix(s, i.name)) {
				value += l * i.value;
				s = StripLeft(end);
				break;
			}
		}
	}

	return value;
}
