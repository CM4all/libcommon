// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <openssl/ossl_typ.h>

#include <string_view>

class AllocatedString;

namespace JWT {

/**
 * Create a JWT ES256 signature.
 *
 * Throws on (OpenSSL/libcrypto) error.
 *
 * @param header_b64 the UrlSafeBase64 of the JWT header
 * @param payload_b64 the UrlSafeBase64 of the payload
 * @return UrlSafeBase64 of the RSA signature
 *
 * @see RFC 7518 section 3.4
 */
AllocatedString
SignES256(EVP_PKEY &key, std::string_view header_b64,
	  std::string_view payload_b64);

} // namespace JWT
