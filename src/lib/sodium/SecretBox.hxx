// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "SecretBoxTypes.hxx"

#include <sodium/crypto_secretbox.h>

static_assert(sizeof(CryptoSecretBoxKey) == crypto_secretbox_KEYBYTES);
static_assert(sizeof(CryptoSecretBoxNonce) == crypto_secretbox_NONCEBYTES);

static inline void
crypto_secretbox_easy(std::byte *ciphertext,
		      std::span<const std::byte> message,
		      CryptoSecretBoxNonceView nonce,
		      CryptoSecretBoxKeyView key) noexcept
{
	crypto_secretbox_easy(reinterpret_cast<unsigned char *>(ciphertext),
			      reinterpret_cast<const unsigned char *>(message.data()),
			      message.size(),
			      reinterpret_cast<const unsigned char *>(nonce.data()),
			      reinterpret_cast<const unsigned char *>(key.data()));
}

static inline bool
crypto_secretbox_open_easy(std::byte *message,
			   std::span<const std::byte> ciphertext,
			   CryptoSecretBoxNonceView nonce,
			   CryptoSecretBoxKeyView key) noexcept
{
	return crypto_secretbox_open_easy(reinterpret_cast<unsigned char *>(message),
					  reinterpret_cast<const unsigned char *>(ciphertext.data()),
					  ciphertext.size(),
					  reinterpret_cast<const unsigned char *>(nonce.data()),
					  reinterpret_cast<const unsigned char *>(key.data())) == 0;
}
