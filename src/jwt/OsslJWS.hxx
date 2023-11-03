// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/ossl_typ.h>

#include <string_view>

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

} // namespace JWS
