// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/openssl/EvpDigest.hxx"
#include "util/HexFormat.hxx"

#include <gtest/gtest.h>

TEST(TestEvpDigest, SHA1)
{
	EvpSHA1Context ctx;
	ctx.Update(std::string_view{"The quick brown fox"});
	ctx.Update(std::string_view{" jumps over the lazy dog"});

	const auto digest = ctx.Final();
	const auto hex = HexFormat(std::span{digest});
	EXPECT_EQ(std::string_view(hex.data(), hex.size()),
		  "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}
