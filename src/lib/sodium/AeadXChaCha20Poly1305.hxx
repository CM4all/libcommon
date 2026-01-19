// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "XChaCha20Types.hxx"

#include <sodium/crypto_aead_xchacha20poly1305.h>

#include <cstddef>
#include <span>

static_assert(sizeof(XChaCha20Key) == crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
static_assert(sizeof(XChaCha20Nonce) == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);

static inline void
crypto_aead_xchacha20poly1305_ietf_encrypt(std::byte *c,
					   std::span<const std::byte> m,
					   std::span<const std::byte> ad,
					   XChaCha20NonceView npub,
					   XChaCha20KeyView k) noexcept
{
	crypto_aead_xchacha20poly1305_ietf_encrypt(reinterpret_cast<unsigned char *>(c),
						   nullptr,
						   reinterpret_cast<const unsigned char *>(m.data()),
						   m.size(),
						   reinterpret_cast<const unsigned char *>(ad.data()),
						   ad.size(),
						   nullptr,
						   reinterpret_cast<const unsigned char *>(npub.data()),
						   reinterpret_cast<const unsigned char *>(k.data()));
}

[[nodiscard]]
static inline int
crypto_aead_xchacha20poly1305_ietf_decrypt(std::byte *m,
					   std::span<const std::byte> c,
					   std::span<const std::byte> ad,
					   XChaCha20NonceView npub,
					   XChaCha20KeyView k) noexcept
{
	return crypto_aead_xchacha20poly1305_ietf_decrypt(reinterpret_cast<unsigned char *>(m),
							  nullptr,
							  nullptr,
							  reinterpret_cast<const unsigned char *>(c.data()),
							  c.size(),
							  reinterpret_cast<const unsigned char *>(ad.data()),
							  ad.size(),
							  reinterpret_cast<const unsigned char *>(npub.data()),
							  reinterpret_cast<const unsigned char *>(k.data()));
}
