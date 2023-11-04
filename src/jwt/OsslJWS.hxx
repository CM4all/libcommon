// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/ossl_typ.h>

#include <string_view>

class AllocatedString;

namespace JWS {

/**
 * Returns the "alg" (Algorithm) Header Parameter Values for JWS for
 * the specified key.  The digest algorithm is assumed to be SHA2-256.
 *
 * Throws on error.
 *
 * @see RFC 7518 section 3.1
 */
std::string_view
GetAlg(const EVP_PKEY &key);

/**
 * Create a JWS signature with the specified EVP_PKEY.
 *
 * Throws on error.
 *
 * @param header_b64 the UrlSafeBase64 of the JWT header
 * @param payload_b64 the UrlSafeBase64 of the payload
 * @return UrlSafeBase64 of the RSA signature
 */
AllocatedString
Sign(EVP_PKEY &key, std::string_view header_b64,
     std::string_view payload_b64);

} // namespace JWS
