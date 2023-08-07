// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/nettle/SHA1.hxx"
#include "util/HexFormat.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <array>

using std::string_view_literals::operator""sv;

TEST(TestSHA1, Empty)
{
	Nettle::SHA1Ctx ctx;
	const auto digest = ctx.Digest();
	const auto hash = HexFormat(std::span{digest});
	EXPECT_EQ(ToStringView(hash), "da39a3ee5e6b4b0d3255bfef95601890afd80709"sv);
}

TEST(TestSHA1, FoxDog)
{
	Nettle::SHA1Ctx ctx;
	ctx.Update(AsBytes("The quick brown fox jumps over the lazy dog"sv));
	const auto digest = ctx.Digest();
	const auto hash = HexFormat(std::span{digest});
	EXPECT_EQ(ToStringView(hash), "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"sv);
}
