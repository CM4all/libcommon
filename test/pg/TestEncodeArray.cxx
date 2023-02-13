// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "../../src/pg/Array.hxx"

#include <gtest/gtest.h>

#include <list>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

TEST(PgTest, EncodeArray)
{
	std::list<std::string> a;
	ASSERT_STREQ(Pg::EncodeArray(a).c_str(), "{}");

	a.push_back("foo");
	ASSERT_STREQ(Pg::EncodeArray(a).c_str(), "{\"foo\"}");

	a.push_back("");
	ASSERT_STREQ(Pg::EncodeArray(a).c_str(), "{\"foo\",\"\"}");

	a.push_back("\\");
	ASSERT_STREQ(Pg::EncodeArray(a).c_str(), "{\"foo\",\"\",\"\\\\\"}");

	a.push_back("\"");
	ASSERT_STREQ(Pg::EncodeArray(a).c_str(), "{\"foo\",\"\",\"\\\\\",\"\\\"\"}");
}
