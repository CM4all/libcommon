// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "jwt/OsslJWK.hxx"
#include "lib/sodium/Base64Alloc.hxx"
#include "lib/openssl/BNAlloc.hxx"
#include "lib/openssl/Error.hxx"
#include "lib/openssl/EvpParam.hxx"
#include "lib/openssl/Key.hxx"
#include "lib/openssl/UniqueBIO.hxx"
#include "lib/openssl/UniqueEVP.hxx"
#include "util/AllocatedArray.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <openssl/core_names.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>

#include <nlohmann/json.hpp>

using std::string_view_literals::operator""sv;

static UniqueEVP_PKEY
LoadPrivateKeyFromPem(std::string_view pem)
{
	UniqueBIO bio{BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()))};
	if (!bio)
		throw SslError{"BIO_new_mem_buf() failed"};

	UniqueEVP_PKEY key{PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr)};
	if (!key)
		throw SslError{"PEM_read_bio_PrivateKey() failed"};

	return key;
}

static AllocatedArray<std::byte>
GetPaddedCoordinate(const EVP_PKEY &key, const char *name, std::size_t size)
{
	const auto coordinate = GetBNParam<false>(key, name);
	return BN_bn2binpad(*coordinate, size);
}

static AllocatedArray<std::byte>
DecodeJWKCoordinate(const nlohmann::json &jwk, std::string_view name)
{
	return DecodeUrlSafeBase64(jwk.at(name).get<std::string_view>());
}

static void
ExpectEcJWK(const EVP_PKEY &key, std::string_view curve_name,
	    std::size_t coordinate_size)
{
	const auto jwk = ToJWK(key);

	EXPECT_EQ(jwk.at("kty"sv).get<std::string_view>(), "EC"sv);
	EXPECT_EQ(jwk.at("crv"sv).get<std::string_view>(), curve_name);

	const auto expected_x = GetPaddedCoordinate(key, OSSL_PKEY_PARAM_EC_PUB_X,
						    coordinate_size);
	const auto expected_y = GetPaddedCoordinate(key, OSSL_PKEY_PARAM_EC_PUB_Y,
						    coordinate_size);
	const auto actual_x = DecodeJWKCoordinate(jwk, "x"sv);
	const auto actual_y = DecodeJWKCoordinate(jwk, "y"sv);

	EXPECT_EQ(ToStringView(actual_x), ToStringView(expected_x));
	EXPECT_EQ(ToStringView(actual_y), ToStringView(expected_y));
}

TEST(JWTOsslJWK, P256)
{
	const auto key = GenerateEcKey(NID_X9_62_prime256v1);
	ExpectEcJWK(*key, "P-256"sv, 32);
}

TEST(JWTOsslJWK, PadsLeadingZeroCoordinate)
{
	static constexpr std::string_view pem =
		"-----BEGIN PRIVATE KEY-----\n"
		"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgcrb4rMCRJjXK3lCz\n"
		"2IQ9HCiXHTpNCzDZ0efw8oNaLymhRANCAAQA5O4DTvA3z+/P2tAJSDnTbaQgcIuJ\n"
		"QK8g9HvXrGEH/o8vwDTU/V9aPYRHWf59Lr8odk0/P365VQWG+zqJngZS\n"
		"-----END PRIVATE KEY-----\n"sv;

	const auto key = LoadPrivateKeyFromPem(pem);
	const auto expected_x = GetPaddedCoordinate(*key, OSSL_PKEY_PARAM_EC_PUB_X, 32);
	const auto expected_y = GetPaddedCoordinate(*key, OSSL_PKEY_PARAM_EC_PUB_Y, 32);
	ASSERT_TRUE(expected_x.front() == std::byte{0} || expected_y.front() == std::byte{0});

	ExpectEcJWK(*key, "P-256"sv, 32);
}
