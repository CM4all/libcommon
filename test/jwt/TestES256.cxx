// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "jwt/ES256.hxx"
#include "lib/openssl/BN.hxx"
#include "lib/openssl/EC.hxx"
#include "lib/openssl/UniqueEC.hxx"
#include "util/AllocatedArray.hxx"

#include <gtest/gtest.h>

#include <openssl/bn.h>
#include <openssl/ec.h>

#include <algorithm>
#include <array>
#include <span>
#include <stdexcept>

static UniqueECDSA_SIG
MakeSignature(std::span<const std::byte> r_bytes,
	      std::span<const std::byte> s_bytes)
{
	return NewUniqueECDSA_SIG(BN_bin2bn<false>(r_bytes),
				  BN_bin2bn<false>(s_bytes));
}

TEST(JWTES256, PadShortR)
{
	std::array<std::byte, 31> r;
	std::array<std::byte, 32> s;

	for (std::size_t i = 0; i < r.size(); ++i)
		r[i] = std::byte{static_cast<unsigned char>(i + 1)};
	for (std::size_t i = 0; i < s.size(); ++i)
		s[i] = std::byte{static_cast<unsigned char>(i + 65)};

	const auto esig = MakeSignature(r, s);
	const auto raw = JWT::EncodeES256Signature(*esig);

	ASSERT_EQ(raw.size(), 64u);
	EXPECT_EQ(raw[0], std::byte{0});
	EXPECT_TRUE(std::equal(r.begin(), r.end(), raw.data() + 1));
	EXPECT_TRUE(std::equal(s.begin(), s.end(), raw.data() + 32));
}

TEST(JWTES256, PadShortS)
{
	std::array<std::byte, 32> r;
	std::array<std::byte, 31> s;

	for (std::size_t i = 0; i < r.size(); ++i)
		r[i] = std::byte{static_cast<unsigned char>(i + 33)};
	for (std::size_t i = 0; i < s.size(); ++i)
		s[i] = std::byte{static_cast<unsigned char>(i + 129)};

	const auto esig = MakeSignature(r, s);
	const auto raw = JWT::EncodeES256Signature(*esig);

	ASSERT_EQ(raw.size(), 64u);
	EXPECT_TRUE(std::equal(r.begin(), r.end(), raw.data()));
	EXPECT_EQ(raw[32], std::byte{0});
	EXPECT_TRUE(std::equal(s.begin(), s.end(), raw.data() + 33));
}

TEST(JWTES256, RejectOversizedComponent)
{
	std::array<std::byte, 33> r;
	std::array<std::byte, 32> s;

	for (std::size_t i = 0; i < r.size(); ++i)
		r[i] = std::byte{static_cast<unsigned char>(i + 1)};
	for (std::size_t i = 0; i < s.size(); ++i)
		s[i] = std::byte{static_cast<unsigned char>(i + 97)};

	const auto esig = MakeSignature(r, s);
	EXPECT_THROW((void)JWT::EncodeES256Signature(*esig), std::invalid_argument);
}
