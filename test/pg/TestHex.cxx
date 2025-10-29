// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "pg/Hex.hxx"
#include "util/AllocatedArray.hxx"
#include "util/AllocatedString.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <stdexcept>

using std::string_view_literals::operator""sv;

TEST(PgTest, DecodeHex)
{
	EXPECT_THROW(Pg::DecodeHex({}), std::invalid_argument);
	EXPECT_THROW(Pg::DecodeHex(""sv), std::invalid_argument);
	EXPECT_THROW(Pg::DecodeHex("\\"sv), std::invalid_argument);
	EXPECT_THROW(Pg::DecodeHex("\\x0"sv), std::invalid_argument);
	EXPECT_THROW(Pg::DecodeHex("\\x000"sv), std::invalid_argument);
	EXPECT_THROW(Pg::DecodeHex("\\x0A"sv), std::invalid_argument);
	EXPECT_THROW(Pg::DecodeHex("\\x00\0"sv), std::invalid_argument);

	EXPECT_EQ(ToStringView(Pg::DecodeHex("\\x"sv)), ""sv);
	EXPECT_EQ(ToStringView(Pg::DecodeHex("\\x00"sv)), "\x00"sv);
	EXPECT_EQ(ToStringView(Pg::DecodeHex("\\x0a"sv)), "\x0a"sv);
	EXPECT_EQ(ToStringView(Pg::DecodeHex("\\x41"sv)), "A"sv);
	EXPECT_EQ(ToStringView(Pg::DecodeHex("\\x410042"sv)), "A\0B"sv);
}

TEST(PgTest, EncodeHex)
{
	EXPECT_STREQ(Pg::EncodeHex(AsBytes(""sv)).c_str(), "\\x");
	EXPECT_STREQ(Pg::EncodeHex(AsBytes("\x00"sv)).c_str(), "\\x00");
	EXPECT_STREQ(Pg::EncodeHex(AsBytes("\x0a"sv)).c_str(), "\\x0a");
	EXPECT_STREQ(Pg::EncodeHex(AsBytes("A"sv)).c_str(), "\\x41");
	EXPECT_STREQ(Pg::EncodeHex(AsBytes("A\0B"sv)).c_str(), "\\x410042");
}
