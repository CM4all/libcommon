/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Key.hxx"
#include "Error.hxx"
#include "UniqueBN.hxx"
#include "UniqueRSA.hxx"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/err.h>

#if OPENSSL_VERSION_NUMBER < 0x30000000L
#include <openssl/dsa.h>
#endif

#include <assert.h>

UniqueEVP_PKEY
GenerateRsaKey()
{
	const UniqueEVP_PKEY_CTX ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
	if (!ctx)
		throw SslError("EVP_PKEY_CTX_new_id() failed");

	if (EVP_PKEY_keygen_init(ctx.get()) <= 0)
		throw SslError("EVP_PKEY_keygen_init() failed");

	if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 4096) <= 0)
		throw SslError("EVP_PKEY_CTX_set_rsa_keygen_bits() failed");

	EVP_PKEY *pkey = nullptr;
	if (EVP_PKEY_keygen(ctx.get(), &pkey) <= 0)
		throw SslError("EVP_PKEY_keygen() failed");

	return UniqueEVP_PKEY(pkey);
}

UniqueEVP_PKEY
DecodeDerKey(std::span<const std::byte> der)
{
	ERR_clear_error();

	auto data = (const unsigned char *)der.data();
	UniqueEVP_PKEY key(d2i_AutoPrivateKey(nullptr, &data, der.size()));
	if (!key)
		throw SslError("d2i_AutoPrivateKey() failed");

	return key;
}

#if OPENSSL_VERSION_NUMBER < 0x30000000L

static bool
MatchModulus(RSA &key1, RSA &key2) noexcept
{
	const BIGNUM *n1, *n2;

	RSA_get0_key(&key1, &n1, nullptr, nullptr);
	RSA_get0_key(&key2, &n2, nullptr, nullptr);

	return BN_cmp(n1, n2) == 0;
}

static bool
MatchModulus(DSA &key1, DSA &key2) noexcept
{
	const BIGNUM *n1, *n2;

	DSA_get0_key(&key1, &n1, nullptr);
	DSA_get0_key(&key2, &n2, nullptr);

	return BN_cmp(n1, n2) == 0;
}

#endif

/**
 * Are both public keys equal?
 */
bool
MatchModulus(EVP_PKEY &key1, EVP_PKEY &key2) noexcept
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	return EVP_PKEY_eq(&key1, &key2) == 1;
#else
	if (EVP_PKEY_base_id(&key1) != EVP_PKEY_base_id(&key2))
		return false;

	switch (EVP_PKEY_base_id(&key1)) {
	case EVP_PKEY_RSA:
		return MatchModulus(*EVP_PKEY_get0_RSA(&key1),
				    *EVP_PKEY_get0_RSA(&key2));

	case EVP_PKEY_DSA:
		return MatchModulus(*EVP_PKEY_get0_DSA(&key1),
				    *EVP_PKEY_get0_DSA(&key2));

	default:
		return false;
	}
#endif
}

/**
 * Does the certificate belong to the given key?
 */
bool
MatchModulus(X509 &cert, EVP_PKEY &key) noexcept
{
	UniqueEVP_PKEY public_key(X509_get_pubkey(&cert));
	if (public_key == nullptr)
		return false;

	return MatchModulus(*public_key, key);
}
