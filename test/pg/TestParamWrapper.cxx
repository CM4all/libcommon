/*
 * Copyright 2022 CM4all GmbH
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

#include "pg/ParamWrapper.hxx"

#include <gtest/gtest.h>

#include <list>
#include <forward_list>
#include <vector>

TEST(PgParamWrapperTest, CString)
{
#if 0
	// TODO implement
	const Pg::ParamWrapper a{"foo"};
	ASSERT_FALSE(a.IsBinary());
	ASSERT_STREQ(a.GetValue(), "foo");
	ASSERT_EQ(a.GetSize(), 0U);
#endif

	const char *value = "foo";
	const Pg::ParamWrapper b{value};
	ASSERT_FALSE(b.IsBinary());
	ASSERT_STREQ(b.GetValue(), "foo");
	ASSERT_EQ(b.GetSize(), 0U);

	value = nullptr;
	const Pg::ParamWrapper c{value};
	ASSERT_FALSE(c.IsBinary());
	ASSERT_EQ(c.GetValue(), nullptr);
	ASSERT_EQ(c.GetSize(), 0U);
}

TEST(PgParamWrapperTest, String)
{
	std::string value{"foo"};
	const Pg::ParamWrapper a{value};
	ASSERT_FALSE(a.IsBinary());
	ASSERT_STREQ(a.GetValue(), "foo");
	ASSERT_EQ(a.GetSize(), 0U);

	value.clear();
	const Pg::ParamWrapper b{value};
	ASSERT_FALSE(b.IsBinary());
	ASSERT_STREQ(b.GetValue(), "");
	ASSERT_EQ(b.GetSize(), 0U);
}

TEST(PgParamWrapperTest, Int)
{
	int value = 42;
	const Pg::ParamWrapper a{value};
	ASSERT_FALSE(a.IsBinary());
	ASSERT_STREQ(a.GetValue(), "42");
	ASSERT_EQ(a.GetSize(), 0U);

	value = 0;
	const Pg::ParamWrapper b{value};
	ASSERT_FALSE(b.IsBinary());
	ASSERT_STREQ(b.GetValue(), "0");
	ASSERT_EQ(b.GetSize(), 0U);

	value = -1;
	const Pg::ParamWrapper c{value};
	ASSERT_FALSE(c.IsBinary());
	ASSERT_STREQ(c.GetValue(), "-1");
	ASSERT_EQ(c.GetSize(), 0U);
}

TEST(PgParamWrapperTest, Bool)
{
	bool value = true;
	const Pg::ParamWrapper a{value};
	ASSERT_FALSE(a.IsBinary());
	ASSERT_STREQ(a.GetValue(), "t");
	ASSERT_EQ(a.GetSize(), 0U);

	value = false;
	const Pg::ParamWrapper b{value};
	ASSERT_FALSE(b.IsBinary());
	ASSERT_STREQ(b.GetValue(), "f");
	ASSERT_EQ(b.GetSize(), 0U);
}

TEST(PgParamWrapperTest, ListOfStrings)
{
	std::list<std::string> l;

	const Pg::ParamWrapper a{l};
	ASSERT_FALSE(a.IsBinary());
	ASSERT_STREQ(a.GetValue(), "{}");
	ASSERT_EQ(a.GetSize(), 0U);

	l.push_back("foo");

	const Pg::ParamWrapper b{l};
	ASSERT_FALSE(b.IsBinary());
	ASSERT_STREQ(b.GetValue(), "{\"foo\"}");
	ASSERT_EQ(b.GetSize(), 0U);
}

TEST(PgParamWrapperTest, VectorOfStrings)
{
	std::vector<std::string> l;

	const Pg::ParamWrapper a{l};
	ASSERT_FALSE(a.IsBinary());
	ASSERT_STREQ(a.GetValue(), "{}");
	ASSERT_EQ(a.GetSize(), 0U);

	l.push_back("foo");

	const Pg::ParamWrapper b{l};
	ASSERT_FALSE(b.IsBinary());
	ASSERT_STREQ(b.GetValue(), "{\"foo\"}");
	ASSERT_EQ(b.GetSize(), 0U);
}

TEST(PgParamWrapperTest, OptionalInt)
{
	std::optional<int> value;

	const Pg::ParamWrapper a{value};
	ASSERT_FALSE(a.IsBinary());
	ASSERT_EQ(a.GetValue(), nullptr);
	ASSERT_EQ(a.GetSize(), 0U);

	value.emplace(0);
	const Pg::ParamWrapper b{value};
	ASSERT_FALSE(b.IsBinary());
	ASSERT_STREQ(b.GetValue(), "0");
	ASSERT_EQ(b.GetSize(), 0U);

	value.emplace(-1);
	const Pg::ParamWrapper c{value};
	ASSERT_FALSE(c.IsBinary());
	ASSERT_STREQ(c.GetValue(), "-1");
	ASSERT_EQ(c.GetSize(), 0U);
}

TEST(PgParamWrapperTest, OptionalString)
{
	std::optional<std::string> value;

	const Pg::ParamWrapper a{value};
	ASSERT_FALSE(a.IsBinary());
	ASSERT_EQ(a.GetValue(), nullptr);
	ASSERT_EQ(a.GetSize(), 0U);

	value.emplace();
	const Pg::ParamWrapper b{value};
	ASSERT_FALSE(b.IsBinary());
	ASSERT_STREQ(b.GetValue(), "");
	ASSERT_EQ(b.GetSize(), 0U);

	value.emplace("foo");
	const Pg::ParamWrapper c{value};
	ASSERT_FALSE(c.IsBinary());
	ASSERT_STREQ(c.GetValue(), "foo");
	ASSERT_EQ(c.GetSize(), 0U);
}

TEST(PgParamWrapperTest, OptionalListOfStrings)
{
	std::optional<std::forward_list<std::string>> value;

	const Pg::ParamWrapper a{value};
	ASSERT_FALSE(a.IsBinary());
	ASSERT_EQ(a.GetValue(), nullptr);
	ASSERT_EQ(a.GetSize(), 0U);

	value.emplace();
	const Pg::ParamWrapper b{value};
	ASSERT_FALSE(b.IsBinary());
	ASSERT_STREQ(b.GetValue(), "{}");
	ASSERT_EQ(b.GetSize(), 0U);

	value->emplace_front("foo");
	const Pg::ParamWrapper c{value};
	ASSERT_FALSE(c.IsBinary());
	ASSERT_STREQ(c.GetValue(), "{\"foo\"}");
	ASSERT_EQ(c.GetSize(), 0U);
}
