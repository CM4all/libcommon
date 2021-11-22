/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "lib/pcre/UniqueRegex.hxx"

#include <gtest/gtest.h>

#include <string.h>

TEST(RegexTest, Match1)
{
	UniqueRegex r;
	ASSERT_FALSE(r.IsDefined());
	r.Compile(".", false, false);
	ASSERT_TRUE(r.IsDefined());
	ASSERT_TRUE(r.Match("a"));
	ASSERT_TRUE(r.Match("abc"));
}

TEST(RegexTest, Match2)
{
	UniqueRegex r = UniqueRegex();
	ASSERT_FALSE(r.IsDefined());
	r.Compile("..", false, false);
	ASSERT_TRUE(r.IsDefined());
	ASSERT_FALSE(r.Match("a"));
	ASSERT_TRUE(r.Match("abc"));
}

TEST(RegexTest, NotAnchored)
{
	UniqueRegex r = UniqueRegex();
	ASSERT_FALSE(r.IsDefined());
	r.Compile("/foo/", false, false);
	ASSERT_TRUE(r.IsDefined());
	ASSERT_TRUE(r.Match("/foo/"));
	ASSERT_TRUE(r.Match("/foo/bar"));
	ASSERT_TRUE(r.Match("foo/foo/"));
}

TEST(RegexTest, Anchored)
{
	UniqueRegex r = UniqueRegex();
	ASSERT_FALSE(r.IsDefined());
	r.Compile("/foo/", true, false);
	ASSERT_TRUE(r.IsDefined());
	ASSERT_TRUE(r.Match("/foo/"));
	ASSERT_TRUE(r.Match("/foo/bar"));
	ASSERT_FALSE(r.Match("foo/foo/"));
}

TEST(RegexTest, Capture)
{
	const UniqueRegex r{"/foo/(.*)", true, true};
	ASSERT_TRUE(r.IsDefined());

	{
		static constexpr auto s = "/foo/";
		const auto m = r.MatchCapture(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 5);
		ASSERT_EQ(m[1].size(), 0);
	}

	{
		static constexpr auto s = "/foo/bar";
		const auto m = r.MatchCapture(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 5);
		ASSERT_EQ(m[1].size(), strlen(s + 5));
	}
}

TEST(RegexTest, CaptureOptional)
{
	const UniqueRegex r{"/foo/(.+)?", true, true};
	ASSERT_TRUE(r.IsDefined());

	{
		static constexpr auto s = "/foo/";
		const auto m = r.MatchCapture(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 2U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), nullptr);
	}

	{
		static constexpr auto s = "/foo/bar";
		const auto m = r.MatchCapture(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 2U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 5);
		ASSERT_EQ(m[1].size(), strlen(s + 5));
	}
}
