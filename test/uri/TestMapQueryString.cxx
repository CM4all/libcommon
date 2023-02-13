// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "uri/MapQueryString.hxx"

#include <gtest/gtest.h>

#include <stdexcept>

TEST(TestMapQueryString, Empty)
{
	const auto m = MapQueryString("");
	EXPECT_TRUE(m.empty());
}

TEST(TestMapQueryString, NoValue)
{
	const auto m = MapQueryString("foo");
	EXPECT_FALSE(m.empty());
	EXPECT_EQ(std::distance(m.begin(), m.end()), std::size_t(1));
	EXPECT_EQ(m.find("bar"), m.end());
	EXPECT_NE(m.find("foo"), m.end());
	EXPECT_STREQ(m.find("foo")->second.c_str(), "");
}

TEST(TestMapQueryString, EmptyValue)
{
	const auto m = MapQueryString("foo=");
	EXPECT_FALSE(m.empty());
	EXPECT_EQ(std::distance(m.begin(), m.end()), std::size_t(1));
	EXPECT_EQ(m.find("bar"), m.end());
	EXPECT_NE(m.find("foo"), m.end());
	EXPECT_STREQ(m.find("foo")->second.c_str(), "");
}

TEST(TestMapQueryString, SingleValue)
{
	const auto m = MapQueryString("foo=bar");
	EXPECT_FALSE(m.empty());
	EXPECT_EQ(std::distance(m.begin(), m.end()), std::size_t(1));
	EXPECT_EQ(m.find("bar"), m.end());
	EXPECT_NE(m.find("foo"), m.end());
	EXPECT_STREQ(m.find("foo")->second.c_str(), "bar");
}

TEST(TestMapQueryString, MultiValue)
{
	const auto m = MapQueryString("foo=bar&foo=a");
	EXPECT_FALSE(m.empty());
	EXPECT_EQ(std::distance(m.begin(), m.end()), std::size_t(2));
	const auto r = m.equal_range("foo");
	EXPECT_EQ(std::distance(r.first, r.second), std::size_t(2));
	EXPECT_STREQ(r.first->second.c_str(), "bar");
	EXPECT_STREQ(std::next(r.first)->second.c_str(), "a");
}

TEST(TestMapQueryString, Escaped)
{
	const auto m = MapQueryString("foo=a%20b%ff");
	EXPECT_FALSE(m.empty());
	EXPECT_EQ(std::distance(m.begin(), m.end()), std::size_t(1));
	EXPECT_NE(m.find("foo"), m.end());
	EXPECT_STREQ(m.find("foo")->second.c_str(), "a b\xff");
}

TEST(TestMapQueryString, PlusEscaped)
{
	const auto m = MapQueryString("foo=+a+b+&bar=++");
	EXPECT_FALSE(m.empty());
	EXPECT_EQ(std::distance(m.begin(), m.end()), std::size_t(2));
	EXPECT_NE(m.find("foo"), m.end());
	EXPECT_STREQ(m.find("foo")->second.c_str(), " a b ");
	EXPECT_NE(m.find("bar"), m.end());
	EXPECT_STREQ(m.find("bar")->second.c_str(), "  ");
}

TEST(TestMapQueryString, BadEscape)
{
	EXPECT_ANY_THROW(MapQueryString("foo=a%"));
	EXPECT_ANY_THROW(MapQueryString("foo=a%f"));
	EXPECT_ANY_THROW(MapQueryString("foo=a%fg"));
	EXPECT_ANY_THROW(MapQueryString("foo=a%gf"));
}
