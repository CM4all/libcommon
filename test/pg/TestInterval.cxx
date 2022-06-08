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
