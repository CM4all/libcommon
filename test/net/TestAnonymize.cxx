/*
 * Copyright 2007-2020 CM4all GmbH
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
#include "util/StringView.hxx"

#include <gtest/gtest.h>

#include <string>

std::ostream &
operator<<(std::ostream &os, StringView s)
{
	return os << std::string(s.data, s.size);
}

void
PrintTo(StringView s, std::ostream* os)
{
	*os << '"' << s << '"';
}

void
PrintTo(std::pair<StringView, StringView> p, std::ostream* os)
{
	*os << '"' << p.first << p.second << '"';
}

bool
operator==(std::pair<StringView, StringView> a, const char *b) noexcept
{
	std::string c(a.first.data, a.first.size);
	c.append(a.second.data, a.second.size);
	return c == b;
}

TEST(Anonymize, Other)
{
	EXPECT_EQ(AnonymizeAddress("foo"), "foo");
	EXPECT_EQ(AnonymizeAddress("foo.example.com"), "foo.example.com");
}

TEST(Anonymize, IPv4)
{
	EXPECT_EQ(AnonymizeAddress("1.2.3.4"), "1.2.3.0");
	EXPECT_EQ(AnonymizeAddress("123.123.123.123"), "123.123.123.0");
}

TEST(Anonymize, IPv6)
{
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5:6:7:8"), "1:2:3:4::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5:6:7::"), "1:2:3:4::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5::"), "1:2:3:4::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4::"), "1:2:3:4::");
	EXPECT_EQ(AnonymizeAddress("1:2:3::"), "1:2:3::");
	EXPECT_EQ(AnonymizeAddress("1:2::"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1::"), "1::");
	EXPECT_EQ(AnonymizeAddress("::1"), "::");
	EXPECT_EQ(AnonymizeAddress("1:2:3::6:7:8"), "1:2:3::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5::7:8"), "1:2:3:4::");
	EXPECT_EQ(AnonymizeAddress("1:2:3:4:5::8"), "1:2:3:4::");
	EXPECT_EQ(AnonymizeAddress("1:2::8"), "1:2::");
	EXPECT_EQ(AnonymizeAddress("1:2::7:8"), "1:2::");
}
