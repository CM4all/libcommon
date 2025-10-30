/*
 * Unit tests for src/util/
 */

#include "util/StringList.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(StringList, StringListContains)
{
	EXPECT_TRUE(StringListContains("foo\0bar"sv, '\0', "foo"sv));
	EXPECT_TRUE(StringListContains("foo\0bar"sv, '\0', "bar"sv));
	EXPECT_TRUE(StringListContains("foo\0bar\0x"sv, '\0', "bar"sv));
	EXPECT_FALSE(StringListContains("foo\0bar"sv, '\0', "f"sv));
	EXPECT_FALSE(StringListContains("foo\0bar"sv, '\0', "fo"sv));
	EXPECT_FALSE(StringListContains("foo\0bar"sv, '\0', "oo"sv));

	EXPECT_TRUE(StringListContains("foo,bar"sv, ',', "foo"sv));
	EXPECT_TRUE(StringListContains("foo,bar"sv, ',', "bar"sv));
	EXPECT_FALSE(StringListContains("foo,bar"sv, ',', "f"sv));
	EXPECT_FALSE(StringListContains("foo,bar"sv, ',', "fo"sv));
	EXPECT_FALSE(StringListContains("foo,bar"sv, ',', "oo"sv));
}
