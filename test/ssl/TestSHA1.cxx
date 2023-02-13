// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/openssl/SHA1.hxx"

#include <gtest/gtest.h>

TEST(TestSHA1, StringView)
{
	auto hex = EvpSHA1_Hex("");
	EXPECT_EQ(std::string_view(hex.data(), hex.size()),
		  "da39a3ee5e6b4b0d3255bfef95601890afd80709");

	hex = EvpSHA1_Hex("The quick brown fox jumps over the lazy dog");
	EXPECT_EQ(std::string_view(hex.data(), hex.size()),
		  "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}
