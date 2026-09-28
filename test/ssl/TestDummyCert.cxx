// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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

	EXPECT_EQ(X509_check_private_key(cert1.get(), key1.get()), 1);
	EXPECT_EQ(X509_check_private_key(cert2.get(), key2.get()), 1);
	EXPECT_EQ(X509_check_private_key(cert1.get(), key2.get()), 0);
	EXPECT_EQ(X509_check_private_key(cert2.get(), key1.get()), 0);
}

TEST(TestDummyCert, EC)
{
	const auto key1 = GenerateEcKey();
	const auto cert1 = MakeSelfSignedDummyCert(*key1, "foo");

	const auto key2 = GenerateEcKey();
	const auto cert2 = MakeSelfSignedDummyCert(*key2, "foo");

	EXPECT_EQ(X509_check_private_key(cert1.get(), key1.get()), 1);
	EXPECT_EQ(X509_check_private_key(cert2.get(), key2.get()), 1);
	EXPECT_EQ(X509_check_private_key(cert1.get(), key2.get()), 0);
	EXPECT_EQ(X509_check_private_key(cert2.get(), key1.get()), 0);
}
