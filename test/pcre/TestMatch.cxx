// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 5);
		ASSERT_EQ(m[1].size(), 0);
	}

	{
		static constexpr auto s = "/foo/bar";
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 5);
		ASSERT_EQ(m[1].size(), strlen(s + 5));
	}
}

TEST(RegexTest, CaptureEmpty)
{
	const UniqueRegex r{"/fo(o?)", true, true};
	ASSERT_TRUE(r.IsDefined());

	{
		static constexpr auto s = "/foo";
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 2U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 3);
		ASSERT_EQ(m[1].size(), 1U);
	}

	{
		static constexpr auto s = "/fo";
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 2U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 3);
		ASSERT_EQ(m[1].size(), 0U);
	}
}

TEST(RegexTest, CaptureOptional)
{
	const UniqueRegex r{"/foo/(.+)?", true, true};
	ASSERT_TRUE(r.IsDefined());

	{
		static constexpr auto s = "/foo/";
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 2U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), nullptr);
		ASSERT_EQ(m[1].size(), 0U);
	}

	{
		static constexpr auto s = "/foo/bar";
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 2U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 5);
		ASSERT_EQ(m[1].size(), strlen(s + 5));
	}
}

TEST(RegexTest, CaptureOptional2)
{
	const UniqueRegex r{"/fo(o)?/(.+)?", true, true};
	ASSERT_TRUE(r.IsDefined());

	{
		static constexpr auto s = "/foo/bar";
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 3U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 3);
		ASSERT_EQ(m[1].size(), 1U);
		ASSERT_EQ(m[2].data(), s + 5);
		ASSERT_EQ(m[2].size(), strlen(s + 5));
	}

	{
		static constexpr auto s = "/fo/bar";
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 3U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), nullptr);
		ASSERT_EQ(m[1].size(), 0U);
		ASSERT_EQ(m[2].data(), s + 4);
		ASSERT_EQ(m[2].size(), strlen(s + 4));
	}

	{
		static constexpr auto s = "/foo/";
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 3U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), s + 3);
		ASSERT_EQ(m[1].size(), 1U);
		ASSERT_EQ(m[2].data(), nullptr);
		ASSERT_EQ(m[2].size(), 0U);
	}

	{
		static constexpr auto s = "/fo/";
		const auto m = r.Match(s);
		ASSERT_TRUE(m);
		ASSERT_EQ(m.size(), 3U);
		ASSERT_EQ(m[0].data(), s);
		ASSERT_EQ(m[0].size(), strlen(s));
		ASSERT_EQ(m[1].data(), nullptr);
		ASSERT_EQ(m[1].size(), 0U);
		ASSERT_EQ(m[2].data(), nullptr);
		ASSERT_EQ(m[2].size(), 0U);
	}
}
