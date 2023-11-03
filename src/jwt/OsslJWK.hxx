// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/* JSON Web Signature library */

#pragma once

#include <openssl/ossl_typ.h>

#include <nlohmann/json_fwd.hpp>

/**
 * Generate a JWK from the specified OpenSSL public key.
 *
 * Throws on error.
 */
nlohmann::json
ToJWK(const EVP_PKEY &key);
