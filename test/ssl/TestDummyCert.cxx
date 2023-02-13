// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/openssl/Dummy.hxx"
#include "lib/openssl/Key.hxx"
#include "util/HexFormat.hxx"

#include <gtest/gtest.h>

TEST(TestDummyCert, RSA)
{
	const auto key1 = GenerateRsaKey(1024);
	const auto cert1 = MakeSelfSignedDummyCert(*key1, "foo");

	const auto key2 = GenerateRsaKey(1024);
	const auto cert2 = MakeSelfSignedDummyCert(*key2, "foo");

	EXPECT_TRUE(MatchModulus(*cert1, *key1));
	EXPECT_TRUE(MatchModulus(*cert2, *key2));
	EXPECT_FALSE(MatchModulus(*cert1, *key2));
	EXPECT_FALSE(MatchModulus(*cert2, *key1));
}

TEST(TestDummyCert, EC)
{
	const auto key1 = GenerateEcKey();
	const auto cert1 = MakeSelfSignedDummyCert(*key1, "foo");

	const auto key2 = GenerateEcKey();
	const auto cert2 = MakeSelfSignedDummyCert(*key2, "foo");

	EXPECT_TRUE(MatchModulus(*cert1, *key1));
	EXPECT_TRUE(MatchModulus(*cert2, *key2));
	EXPECT_FALSE(MatchModulus(*cert1, *key2));
	EXPECT_FALSE(MatchModulus(*cert2, *key1));
}
