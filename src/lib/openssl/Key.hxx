// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
GenerateEcKey();

/**
 * Decode a private key encoded with DER.  It is a wrapper for
 * d2i_AutoPrivateKey().
 *
 * Throws SslError on error.
 */
UniqueEVP_PKEY
DecodeDerKey(std::span<const std::byte> der);

[[gnu::pure]]
bool
MatchModulus(EVP_PKEY &key1, EVP_PKEY &key2) noexcept;

[[gnu::pure]]
bool
MatchModulus(X509 &cert, EVP_PKEY &key) noexcept;
