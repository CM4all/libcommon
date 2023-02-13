// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "../../src/pg/Timestamp.hxx"

#include <gtest/gtest.h>

TEST(PgTest, ParseTimestamp)
{
	ASSERT_EQ(Pg::ParseTimestamp("1970-01-01 00:00:00+00"),
		  std::chrono::system_clock::from_time_t(0));
	ASSERT_EQ(Pg::ParseTimestamp("1970-01-01 00:00:00.05+00"),
		  std::chrono::system_clock::time_point(std::chrono::milliseconds(50)));
	ASSERT_EQ(Pg::ParseTimestamp("2009-02-13 23:31:30Z"),
		  std::chrono::system_clock::from_time_t(1234567890));

	/* with time zone */

	ASSERT_EQ(Pg::ParseTimestamp("2009-02-13 23:31:30+02"),
		  std::chrono::system_clock::from_time_t(1234567890)
		  - std::chrono::hours(2));

	ASSERT_EQ(Pg::ParseTimestamp("2009-02-13 23:31:30-01:30"),
		  std::chrono::system_clock::from_time_t(1234567890)
		  + std::chrono::hours(1)
		  + std::chrono::minutes(30));
}
