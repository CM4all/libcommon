/*
 * Copyright 2019-2022 CM4all GmbH
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

#include "time/Parser.hxx"

#include <gtest/gtest.h>

#include <time.h>

TEST(Parser, Today)
{
	const time_t now = time(nullptr);
	struct tm tm;
	localtime_r(&now, &tm);
	tm.tm_sec = 0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	const time_t expected = mktime(&tm);

	auto result = ParseTimePoint("today");
	EXPECT_NEAR(std::chrono::system_clock::to_time_t(result.first), expected, 10);
	EXPECT_EQ(result.second,
		  std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::hours(24)));

	result = ParseTimePoint("yesterday");
	EXPECT_NEAR(std::chrono::system_clock::to_time_t(result.first), expected - 24 * 3600, 10);
	EXPECT_EQ(result.second,
		  std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::hours(24)));

	result = ParseTimePoint("tomorrow");
	EXPECT_NEAR(std::chrono::system_clock::to_time_t(result.first), expected + 24 * 3600, 10);
	EXPECT_EQ(result.second,
		  std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::hours(24)));
}

TEST(Parser, Relative)
{
	constexpr double error = std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds{5}).count();

	EXPECT_NEAR(ParseTimePoint("+1h").first.time_since_epoch().count(),
		    (std::chrono::system_clock::now() + std::chrono::hours{1}).time_since_epoch().count(),
		    error);
	EXPECT_EQ(ParseTimePoint("+1h").second, std::chrono::hours{1});

	EXPECT_NEAR(ParseTimePoint("-1h").first.time_since_epoch().count(),
		    (std::chrono::system_clock::now() - std::chrono::hours{1}).time_since_epoch().count(),
		    error);
	EXPECT_EQ(ParseTimePoint("-1h").second, std::chrono::hours{1});

	EXPECT_NEAR(ParseTimePoint("-60m").first.time_since_epoch().count(),
		    (std::chrono::system_clock::now() - std::chrono::hours{1}).time_since_epoch().count(),
		    error);
	EXPECT_EQ(ParseTimePoint("-60m").second, std::chrono::minutes{1});

	EXPECT_NEAR(ParseTimePoint("-60s").first.time_since_epoch().count(),
		    (std::chrono::system_clock::now() - std::chrono::minutes{1}).time_since_epoch().count(),
		    error);
	EXPECT_EQ(ParseTimePoint("-60s").second, std::chrono::seconds{1});

	EXPECT_NEAR(ParseTimePoint("+20s").first.time_since_epoch().count(),
		    (std::chrono::system_clock::now() + std::chrono::seconds{20}).time_since_epoch().count(),
		    error);
	EXPECT_EQ(ParseTimePoint("+20s").second, std::chrono::seconds{1});

	EXPECT_NEAR(ParseTimePoint("-7d").first.time_since_epoch().count(),
		    (std::chrono::system_clock::now() - std::chrono::hours{24 * 7}).time_since_epoch().count(),
		    error);
	EXPECT_EQ(ParseTimePoint("-7d").second, std::chrono::hours{24});
}
