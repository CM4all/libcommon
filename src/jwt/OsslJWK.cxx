// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "OsslJWK.hxx"
#include "lib/sodium/Base64.hxx"
#include "lib/openssl/Buffer.hxx"
#include "util/AllocatedString.hxx"
#include "util/ScopeExit.hxx"

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/core_names.h>

#include <nlohmann/json.hpp>

#include <stdexcept>

using std::string_view_literals::operator""sv;

static nlohmann::json
RSAToJWK(const EVP_PKEY &key)
{
	assert(EVP_PKEY_get_base_id(&key) == EVP_PKEY_RSA);

	BIGNUM *n = nullptr;
	if (!EVP_PKEY_get_bn_param(&key, OSSL_PKEY_PARAM_RSA_N, &n))
		throw std::runtime_error("Failed to get RSA N value");

	AtScopeExit(n) { BN_clear_free(n); };

	BIGNUM *e = nullptr;
	if (!EVP_PKEY_get_bn_param(&key, OSSL_PKEY_PARAM_RSA_E, &e))
		throw std::runtime_error("Failed to get RSA E value");

	AtScopeExit(e) { BN_clear_free(e); };

	return {
		{ "e"sv, UrlSafeBase64(SslBuffer{*e}.get()).c_str() },
		{ "kty"sv, "RSA"sv },
		{ "n"sv, UrlSafeBase64(SslBuffer{*n}.get()).c_str() },
	};
}

nlohmann::json
ToJWK(const EVP_PKEY &key)
{
	switch (EVP_PKEY_get_base_id(&key)) {
	case EVP_PKEY_RSA:
		return RSAToJWK(key);

	default:
		throw std::invalid_argument{"RSA key expected"};
	}
}
