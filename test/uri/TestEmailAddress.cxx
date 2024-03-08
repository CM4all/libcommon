// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "uri/EmailAddress.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(EmailAddress, VerifyEmailAddress)
{
	EXPECT_FALSE(VerifyEmailAddress(""sv));
	EXPECT_FALSE(VerifyEmailAddress("@"sv));
	EXPECT_FALSE(VerifyEmailAddress("foo@"sv));
	EXPECT_FALSE(VerifyEmailAddress("@example.com"sv));

	EXPECT_TRUE(VerifyEmailAddress("foo@example.com"sv));
	EXPECT_TRUE(VerifyEmailAddress("foo+bar@example.com"sv));

	EXPECT_TRUE(VerifyEmailAddress("\"foo@bar\"@example.com"sv));
	EXPECT_TRUE(VerifyEmailAddress("\"foo<bar\"@example.com"sv));
	EXPECT_TRUE(VerifyEmailAddress("\"foo>bar\"@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("\"foo\176bar\"@bar@example.com"sv));

	EXPECT_FALSE(VerifyEmailAddress("foo<bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("foo>bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("foo@bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("foo\176bar@bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("foo\177bar@bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("\"foo\177bar\"@bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("foo\200bar@bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("\"foo\200bar\"@bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("foo bar@bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("\"foo bar\"@bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("foo\0bar@example.com"sv));
	EXPECT_FALSE(VerifyEmailAddress("\"foo\0bar\"@example.com"sv));
}
