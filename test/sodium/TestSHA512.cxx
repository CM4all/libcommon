// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/sodium/SHA512.hxx"
#include "util/HexFormat.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <array>

using std::string_view_literals::operator""sv;

TEST(TestSHA512, Empty)
{
	SHA512State state;
	const auto hash = state.Final();
	const auto hex = HexFormat(std::span{hash});
	EXPECT_EQ(ToStringView(hex), "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"sv);
}

TEST(TestSHA512, Empty1)
{
	const auto hash = SHA512({});
	const auto hex = HexFormat(std::span{hash});
	EXPECT_EQ(ToStringView(hex), "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"sv);
}
