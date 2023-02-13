// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "net/Anonymize.hxx"

#include <gtest/gtest.h>

#include <string>

using std::string_view_literals::operator""sv;

void
PrintTo(std::string_view s, std::ostream* os)
{
	*os << '"' << s << '"';
}

void
PrintTo(std::pair<std::string_view, std::string_view> p, std::ostream* os)
{
	*os << '"' << p.first << p.second << '"';
}

namespace std {

bool
operator==(pair<string_view, string_view> a,
	   const string_view b) noexcept
{
	string c(a.first);
	c.append(a.second);
	return c == b;
}

} // namespace std

TEST(Anonymize, Other)
{
	EXPECT_EQ(AnonymizeAddress("foo"sv), "foo"sv);
	EXPECT_EQ(AnonymizeAddress("foo.example.com"sv), "foo.example.com"sv);
}

TEST(Anonymize, IPv4)
{
	EXPECT_EQ(AnonymizeAddress("1.2.3.4"), "1.2.3.0");
	EXPECT_EQ(AnonymizeAddress("123.123.123.123"), "123.123.123.0");
}

TEST(Anonymize, IPv6)
{
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5:6:7:8"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5:6:7:8"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5:6:7::"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5::"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4::"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2:3::"), "1:2::");

	EXPECT_EQ(AnonymizeAddress("1:2:ab:4:5:6:7:8"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2:abc:4:5:6:7:8"), "1:2:a00::");
	EXPECT_EQ(AnonymizeAddress("1:2:abcd:4:5:6:7:8"), "1:2:ab00::");
	EXPECT_EQ(AnonymizeAddress("1:2:abcd:4:5:6:7::"), "1:2:ab00::");
	EXPECT_EQ(AnonymizeAddress("1:2:abcd:4:5::"), "1:2:ab00::");
	EXPECT_EQ(AnonymizeAddress("1:2:abcd:4::"), "1:2:ab00::");
	EXPECT_EQ(AnonymizeAddress("1:2:abcd::"), "1:2:ab00::");
	EXPECT_EQ(AnonymizeAddress("1:2::"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1::"), "1::");
	EXPECT_EQ(AnonymizeAddress("::1"), "::");
	EXPECT_EQ(AnonymizeAddress("1:2:abcd::6:7:8"), "1:2:ab00::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5::7:8"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5::8"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2::8"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2::7:8"), "1:2::");
}
