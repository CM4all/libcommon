// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/TokenBucket.hxx"

#include <gtest/gtest.h>

#include <array>

TEST(TokenBucket, Check)
{
	constexpr TokenBucketConfig config{.rate = 10, .burst = 50};

	double now = 1234;

	TokenBucket tb;

	EXPECT_FALSE(tb.Check(config, now, config.burst + 0.1));
	EXPECT_TRUE(tb.Check(config, now, config.burst - 0.1));
	EXPECT_FALSE(tb.Check(config, now, 1));

	now += 1;
	EXPECT_TRUE(tb.Check(config, now, config.rate));
	EXPECT_FALSE(tb.Check(config, now, 1));

	now += 1;
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_TRUE(tb.Check(config, now, 1));
	EXPECT_FALSE(tb.Check(config, now, 1));

	now += config.burst * 10;
	EXPECT_FALSE(tb.Check(config, now, config.burst + 0.1));
	EXPECT_TRUE(tb.Check(config, now, config.burst - 0.1));
	EXPECT_FALSE(tb.Check(config, now, 1));
}
