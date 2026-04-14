// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "OsslJWK.hxx"
#include "lib/sodium/Base64Alloc.hxx"
#include "lib/openssl/BNAlloc.hxx"
#include "lib/openssl/EvpParam.hxx"
#include "util/AllocatedArray.hxx"
#include "util/AllocatedString.hxx"

#include <openssl/evp.h>
#include <openssl/core_names.h>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <vector>

using std::string_view_literals::operator""sv;

static nlohmann::json
RSAToJWK(const EVP_PKEY &key)
{
	assert(EVP_PKEY_get_base_id(&key) == EVP_PKEY_RSA);

	const auto n = GetBNParam<false>(key, OSSL_PKEY_PARAM_RSA_N);
	const auto e = GetBNParam<false>(key, OSSL_PKEY_PARAM_RSA_E);

	return {
		{ "e"sv, UrlSafeBase64(BN_bn2bin(*e)).c_str() },
		{ "kty"sv, "RSA"sv },
		{ "n"sv, UrlSafeBase64(BN_bn2bin(*n)).c_str() },
	};
}

struct EcJWKParams {
	const char *curve_name;
	unsigned bits;
};

static constexpr EcJWKParams ec_jwk_params[] = {
	{ "P-256", 256 },
	{ "P-384", 384 },
	{ "P-521", 521 },
};

static const EcJWKParams &
GetEcJWKParams(const EVP_PKEY &key)
{
	const unsigned bits = EVP_PKEY_get_bits(&key);

	for (const auto &i : ec_jwk_params)
		if (i.bits == bits)
			return i;

	throw std::invalid_argument{"Unsupported EC group"};
}

static nlohmann::json
ECToJWK(const EVP_PKEY &key)
{
	assert(EVP_PKEY_get_base_id(&key) == EVP_PKEY_EC);

	const auto params = GetEcJWKParams(key);
	const std::size_t coordinate_size = (params.bits + 7) / 8;

	const auto x = GetBNParam<false>(key, OSSL_PKEY_PARAM_EC_PUB_X);
	const auto y = GetBNParam<false>(key, OSSL_PKEY_PARAM_EC_PUB_Y);

	return {
		{ "kty"sv, "EC"sv },
		{ "crv"sv, params.curve_name },
		{ "x"sv, UrlSafeBase64(BN_bn2binpad(*x, coordinate_size)).c_str() },
		{ "y"sv, UrlSafeBase64(BN_bn2binpad(*y, coordinate_size)).c_str() },
	};
}

nlohmann::json
ToJWK(const EVP_PKEY &key)
{
	switch (EVP_PKEY_get_base_id(&key)) {
	case EVP_PKEY_RSA:
		return RSAToJWK(key);

	case EVP_PKEY_EC:
		return ECToJWK(key);

	default:
		throw std::invalid_argument{"Unsupported key type"};
	}
}
