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
