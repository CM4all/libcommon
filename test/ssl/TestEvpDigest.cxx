// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lib/openssl/EvpDigest.hxx"
#include "util/HexFormat.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(TestEvpDigest, SHA1)
{
	EvpSHA1Context ctx;
	ctx.Update("The quick brown fox"sv);
	ctx.Update(" jumps over the lazy dog"sv);

	const auto digest = ctx.Final();
	const auto hex = HexFormat(std::span{digest});
	EXPECT_EQ(hex, "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"sv);
}
