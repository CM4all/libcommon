// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "OsslJWK.hxx"
#include "lib/sodium/Base64.hxx"
#include "lib/openssl/Buffer.hxx"
#include "lib/openssl/EvpParam.hxx"
#include "util/AllocatedString.hxx"

#include <openssl/evp.h>
#include <openssl/core_names.h>

#include <nlohmann/json.hpp>

#include <stdexcept>

using std::string_view_literals::operator""sv;

static nlohmann::json
RSAToJWK(const EVP_PKEY &key)
{
	assert(EVP_PKEY_get_base_id(&key) == EVP_PKEY_RSA);

	const auto n = GetBNParam<false>(key, OSSL_PKEY_PARAM_RSA_N);
	const auto e = GetBNParam<false>(key, OSSL_PKEY_PARAM_RSA_E);

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
