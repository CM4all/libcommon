// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "http/Date.hxx"

#include <gtest/gtest.h>

TEST(HttpDate, Format)
{
	EXPECT_STREQ(http_date_format(std::chrono::system_clock::time_point{}), "Thu, 01 Jan 1970 00:00:00 GMT");
	EXPECT_STREQ(http_date_format(std::chrono::system_clock::from_time_t(1234567890)), "Fri, 13 Feb 2009 23:31:30 GMT");
}

TEST(HttpDate, Parse)
{
	EXPECT_EQ(std::chrono::system_clock::to_time_t(http_date_parse("Thu, 01 Jan 1970 00:00:00 GMT")), 0);
	EXPECT_EQ(std::chrono::system_clock::to_time_t(http_date_parse("Fri, 13 Feb 2009 23:31:30 GMT")), 1234567890);
	EXPECT_LE(http_date_parse(""), std::chrono::system_clock::time_point{});
	EXPECT_LE(http_date_parse("Thu, 01 Jan 1970"), std::chrono::system_clock::time_point{});
	EXPECT_LE(http_date_parse("1970-01-01T00:00:00Z"), std::chrono::system_clock::time_point{});
}
