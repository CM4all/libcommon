// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

/*
 * OpenSSL key utilities.
 */

#pragma once

#include "UniqueEVP.hxx"

#include <openssl/ossl_typ.h>

#include <span>

UniqueEVP_PKEY
GenerateRsaKey(unsigned bits=4096);

/**
 * Generate a new elliptic curve key.
 *
 * Throws #SslError on error.
 */
UniqueEVP_PKEY
GenerateEcKey(int curve_nid);

/**
 * This overload uses the default curve `NID_X9_62_prime256v1`.
 */
UniqueEVP_PKEY
GenerateEcKey();

/**
 * Decode a private key encoded with DER.  It is a wrapper for
 * d2i_AutoPrivateKey().
 *
 * Throws SslError on error.
 */
UniqueEVP_PKEY
DecodeDerKey(std::span<const std::byte> der);

/**
 * Are both public keys equal?
 */
[[gnu::pure]]
bool
MatchModulus(const EVP_PKEY &key1, const EVP_PKEY &key2) noexcept;

/**
 * Does the certificate belong to the given key?
 */
[[gnu::pure]]
bool
MatchModulus(X509 &cert, const EVP_PKEY &key) noexcept;
