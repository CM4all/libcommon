// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "SignTypes.hxx"

#include <sodium/crypto_sign.h>

static_assert(sizeof(CryptoSignPublicKey) == crypto_sign_PUBLICKEYBYTES);
static_assert(sizeof(CryptoSignSecretKey) == crypto_sign_SECRETKEYBYTES);
static_assert(sizeof(CryptoSignature) == crypto_sign_BYTES);

static inline void
crypto_sign_keypair(CryptoSignPublicKeyPtr pk,
		    CryptoSignSecretKeyPtr sk) noexcept
{
	crypto_sign_keypair(reinterpret_cast<unsigned char *>(pk.data()),
			    reinterpret_cast<unsigned char *>(sk.data()));
}

static inline void
crypto_sign(std::byte *sm,
	    std::span<const std::byte> m,
	    CryptoSignSecretKeyView sk) noexcept
{
	crypto_sign(reinterpret_cast<unsigned char *>(sm), nullptr,
		    reinterpret_cast<const unsigned char *>(m.data()),
		    m.size(),
		    reinterpret_cast<const unsigned char *>(sk.data()));
}

static inline bool
crypto_sign_open(std::byte *m,
		 std::span<const std::byte> sm,
		 CryptoSignPublicKeyView pk) noexcept
{
	return crypto_sign_open(reinterpret_cast<unsigned char *>(m), nullptr,
				reinterpret_cast<const unsigned char *>(sm.data()),
				sm.size(),
				reinterpret_cast<const unsigned char *>(pk.data()));
}

static inline void
crypto_sign_detached(CryptoSignaturePtr sig,
		     std::span<const std::byte> m,
		     CryptoSignSecretKeyView sk) noexcept
{
	crypto_sign_detached(reinterpret_cast<unsigned char *>(sig.data()), nullptr,
			     reinterpret_cast<const unsigned char *>(m.data()),
			     m.size(),
			     reinterpret_cast<const unsigned char *>(sk.data()));
}

[[gnu::pure]]
static inline CryptoSignature
crypto_sign_detached(std::span<const std::byte> m,
		     CryptoSignSecretKeyView sk) noexcept
{
	CryptoSignature sig;
	crypto_sign_detached(sig, m, sk);
	return sig;
}

[[gnu::pure]]
static inline int
crypto_sign_verify_detached(CryptoSignatureView sig,
			    std::span<const std::byte> m,
			    CryptoSignPublicKeyView pk) noexcept
{
	return crypto_sign_verify_detached(reinterpret_cast<const unsigned char *>(sig.data()),
					   reinterpret_cast<const unsigned char *>(m.data()),
					   m.size(),
					   reinterpret_cast<const unsigned char *>(pk.data()));
}
