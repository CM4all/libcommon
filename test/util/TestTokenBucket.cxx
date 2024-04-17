// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/TokenBucket.hxx"

#include <gtest/gtest.h>

#include <array>

TEST(TokenBucket, Check)
{
	constexpr double rate = 10, burst = 50;

	double now = 1234;

	TokenBucket tb;

	EXPECT_FALSE(tb.Check(now, rate, burst, burst + 0.1));
	EXPECT_TRUE(tb.Check(now, rate, burst, burst - 0.1));
	EXPECT_FALSE(tb.Check(now, rate, burst, 1));

	now += 1;
	EXPECT_TRUE(tb.Check(now, rate, burst, rate));
	EXPECT_FALSE(tb.Check(now, rate, burst, 1));

	now += 1;
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_TRUE(tb.Check(now, rate, burst, 1));
	EXPECT_FALSE(tb.Check(now, rate, burst, 1));

	now += burst * 10;
	EXPECT_FALSE(tb.Check(now, rate, burst, burst + 0.1));
	EXPECT_TRUE(tb.Check(now, rate, burst, burst - 0.1));
	EXPECT_FALSE(tb.Check(now, rate, burst, 1));
}
