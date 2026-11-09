// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "MlKem768Types.hxx"

#include <sodium/crypto_kem_mlkem768.h>

static_assert(sizeof(MlKem768PublicKey) == crypto_kem_mlkem768_PUBLICKEYBYTES);
static_assert(sizeof(MlKem768SecretKey) == crypto_kem_mlkem768_SECRETKEYBYTES);
static_assert(sizeof(MlKem768CipherText) == crypto_kem_mlkem768_CIPHERTEXTBYTES);
static_assert(sizeof(MlKem768SharedSecret) == crypto_kem_mlkem768_SHAREDSECRETBYTES);

static inline void
crypto_kem_mlkem768_keypair(MlKem768PublicKeyPtr pk,
			    MlKem768SecretKeyPtr sk) noexcept
{
	crypto_kem_mlkem768_keypair(reinterpret_cast<unsigned char *>(pk.data()),
				    reinterpret_cast<unsigned char *>(sk.data()));
}

[[nodiscard]]
static inline int
crypto_kem_mlkem768_enc(MlKem768CipherTextPtr ct,
			MlKem768SharedSecretPtr ss,
			MlKem768PublicKeyView pk) noexcept
{
	return crypto_kem_mlkem768_enc(reinterpret_cast<unsigned char *>(ct.data()),
				       reinterpret_cast<unsigned char *>(ss.data()),
				       reinterpret_cast<const unsigned char *>(pk.data()));
}

[[nodiscard]]
static inline int
crypto_kem_mlkem768_dec(MlKem768SharedSecretPtr ss,
			MlKem768CipherTextView ct,
			MlKem768SecretKeyView sk) noexcept
{
	return crypto_kem_mlkem768_dec(reinterpret_cast<unsigned char *>(ss.data()),
				       reinterpret_cast<const unsigned char *>(ct.data()),
				       reinterpret_cast<const unsigned char *>(sk.data()));
}
