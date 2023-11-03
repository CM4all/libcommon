// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "OsslJWS.hxx"

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

	default:
		throw std::invalid_argument{"Unsupported key type"};
	}
}

} // namespace JWS
