// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/nettle/SHA3.hxx"
#include "util/HexFormat.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <array>

using std::string_view_literals::operator""sv;

TEST(TestSHA3_256, Empty)
{
	Nettle::SHA3_256Ctx ctx;
	const auto digest = ctx.Digest();
	const auto hash = HexFormat(std::span{digest});
	EXPECT_EQ(ToStringView(hash), "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"sv);
}

TEST(TestSHA3_256, FoxDog)
{
	Nettle::SHA3_256Ctx ctx;
	ctx.Update(AsBytes("The quick brown fox jumps over the lazy dog"sv));
	const auto digest = ctx.Digest();
	const auto hash = HexFormat(std::span{digest});
	EXPECT_EQ(ToStringView(hash), "69070dda01975c8c120c3aada1b282394e7f032fa9cf32f4cb2259a0897dfc04"sv);
}
