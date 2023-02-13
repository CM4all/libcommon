// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
