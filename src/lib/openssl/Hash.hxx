// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Calculate hashes of OpenSSL objects.
 */

#ifndef SSL_HASH_HXX
#define SSL_HASH_HXX

#include <openssl/ossl_typ.h>
#include <openssl/sha.h>

#include <span>

struct SHA1Digest {
	unsigned char data[SHA_DIGEST_LENGTH];
};

[[gnu::pure]]
SHA1Digest
CalcSHA1(std::span<const std::byte> src);

[[gnu::pure]]
SHA1Digest
CalcSHA1(X509_NAME &src);

#endif
