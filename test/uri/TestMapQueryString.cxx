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

#include "uri/MapQueryString.hxx"
#include "util/StringView.hxx"

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

TEST(TestMapQueryString, BadEscape)
{
	EXPECT_ANY_THROW(MapQueryString("foo=a%"));
	EXPECT_ANY_THROW(MapQueryString("foo=a%f"));
	EXPECT_ANY_THROW(MapQueryString("foo=a%fg"));
	EXPECT_ANY_THROW(MapQueryString("foo=a%gf"));
}
