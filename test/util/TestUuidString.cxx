// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "util/UuidString.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(UuidString, IsUuidString)
{
	EXPECT_FALSE(IsUuidString(""sv));
	EXPECT_FALSE(IsUuidString("foo"sv));

	EXPECT_TRUE(IsUuidString("f81d4fae-7dec-11d0-a765-00a0c91e6bf6"sv));

	EXPECT_FALSE(IsUuidString("f81d4fae-7dec-11d0-a765-00a0c91e6bf6 "sv));
	EXPECT_FALSE(IsUuidString("f81d4fae-7dec-11d0-a765-00a0c91e6bg6"sv));
	EXPECT_FALSE(IsUuidString("f81d4fae 7dec-11d0-a765-00a0c91e6bf6"sv));
}
