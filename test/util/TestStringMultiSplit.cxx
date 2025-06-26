// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "util/StringMultiSplit.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(StringMultiSplit, Basic)
{
	auto [a, b, c] = StringMultiSplit<2>("a/b/c"sv, '/');

	EXPECT_EQ(a, "a"sv);
	EXPECT_EQ(b, "b"sv);
	EXPECT_EQ(c, "c"sv);
}

TEST(StringMultiSplit, Empty)
{
	auto [a, b, c] = StringMultiSplit<2>(""sv, '/');

	EXPECT_EQ(a, ""sv);
	EXPECT_NE(a.data(), nullptr);
	EXPECT_EQ(b, ""sv);
	EXPECT_EQ(b.data(), nullptr);
	EXPECT_EQ(c, ""sv);
	EXPECT_EQ(c.data(), nullptr);
}

TEST(StringMultiSplit, Null)
{
	auto [a, b, c] = StringMultiSplit<2>(std::string_view{}, '/');

	EXPECT_EQ(a, ""sv);
	EXPECT_EQ(a.data(), nullptr);
	EXPECT_EQ(b, ""sv);
	EXPECT_EQ(b.data(), nullptr);
	EXPECT_EQ(c, ""sv);
	EXPECT_EQ(c.data(), nullptr);
}

TEST(StringMultiSplit, Short)
{
	auto [a, b, c] = StringMultiSplit<2>("a/bc"sv, '/');

	EXPECT_EQ(a, "a"sv);
	EXPECT_EQ(b, "bc"sv);
	EXPECT_EQ(c, ""sv);
	EXPECT_EQ(c.data(), nullptr);
}

TEST(StringMultiSplit, Long)
{
	auto [a, b, c] = StringMultiSplit<2>("a/b/c/d"sv, '/');

	EXPECT_EQ(a, "a"sv);
	EXPECT_EQ(b, "b"sv);
	EXPECT_EQ(c, "c/d"sv);
}

TEST(StringMultiSplit, Zero)
{
	auto [a] = StringMultiSplit<0>("a/b/c"sv, '/');

	EXPECT_EQ(a, "a/b/c"sv);
}

TEST(StringMultiSplit, One)
{
	auto [a, b] = StringMultiSplit<1>("a/b/c"sv, '/');

	EXPECT_EQ(a, "a"sv);
	EXPECT_EQ(b, "b/c"sv);
}
