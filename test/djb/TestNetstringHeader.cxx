// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "net/djb/NetstringHeader.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(NetstringHeader, Basic)
{
	EXPECT_EQ(NetstringHeader{}(0U), "0:"sv);
	EXPECT_EQ(NetstringHeader{}(1U), "1:"sv);
	EXPECT_EQ(NetstringHeader{}(42U), "42:"sv);
	EXPECT_EQ(NetstringHeader{}(65536U), "65536:"sv);
	EXPECT_EQ(NetstringHeader{}(2147483647U), "2147483647:"sv);
	EXPECT_EQ(NetstringHeader{}(2147483648U), "2147483648:"sv);
	EXPECT_EQ(NetstringHeader{}(4294967295U), "4294967295:"sv);

	if constexpr (sizeof(std::size_t) > 4) {
		EXPECT_EQ(NetstringHeader{}(4294967296ULL), "4294967296:"sv);
		EXPECT_EQ(NetstringHeader{}(18446744073709551615ULL), "18446744073709551615:"sv);
	}
}
