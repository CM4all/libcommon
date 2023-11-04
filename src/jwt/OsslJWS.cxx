// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "OsslJWS.hxx"
#include "RS256.hxx"
#include "util/AllocatedString.hxx"

#include <openssl/evp.h>

#include <stdexcept>

using std::string_view_literals::operator""sv;

namespace JWS {

std::string_view
GetAlg(const EVP_PKEY &key)
{
	switch (EVP_PKEY_get_base_id(&key)) {
	case EVP_PKEY_RSA:
		return "RS256"sv;

	case EVP_PKEY_EC:
		return "ES256"sv;

	default:
		throw std::invalid_argument{"Unsupported key type"};
	}
}

AllocatedString
Sign(EVP_PKEY &key, std::string_view header_b64,
     std::string_view payload_b64)
{
	switch (EVP_PKEY_get_base_id(&key)) {
	case EVP_PKEY_RSA:
		return JWT::SignRS256(key, header_b64, payload_b64);

	default:
		throw std::invalid_argument{"Unsupported key type"};
	}
}

} // namespace JWS
