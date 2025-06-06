// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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
