// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "../../src/pg/Interval.hxx"

#include <gtest/gtest.h>

static constexpr std::chrono::hours pg_day(24);
static constexpr auto pg_month = 30 * pg_day;
static constexpr auto pg_year = 365 * pg_day + pg_day / 4;

TEST(PgTest, ParseIntervalS)
{
	/* example strings from the PostgreSQL 9.6 documentation
	   (https://www.postgresql.org/docs/9.6/static/datatype-datetime.html) */
	ASSERT_EQ(Pg::ParseIntervalS("1 year 2 mons"),
		  pg_year + 2 * pg_month);
	ASSERT_EQ(Pg::ParseIntervalS("3 days 04:05:06"),
		  3 * pg_day + std::chrono::hours(4) + std::chrono::minutes(5) + std::chrono::seconds(6));
	ASSERT_EQ(Pg::ParseIntervalS("-1 year -2 mons +3 days -04:05:06"),
		  -pg_year - 2 * pg_month + 3 * pg_day
		  - std::chrono::hours(4) - std::chrono::minutes(5) - std::chrono::seconds(6));
}
