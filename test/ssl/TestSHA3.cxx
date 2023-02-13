// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/openssl/SHA3.hxx"

#include <gtest/gtest.h>

TEST(TestSHA3, StringView)
{
	const auto hex = EvpSHA3_256_Hex("");
	EXPECT_EQ(std::string_view(hex.data(), hex.size()),
		  "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
}

TEST(TestSHA3, ByteSpan)
{
	const auto hex = EvpSHA3_256_Hex(std::span<std::byte>{});
	EXPECT_EQ(std::string_view(hex.data(), hex.size()),
		  "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
}

TEST(TestSHA3, IntSpan)
{
	const auto hex = EvpSHA3_256_Hex(std::span<int>{});
	EXPECT_EQ(std::string_view(hex.data(), hex.size()),
		  "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
}
