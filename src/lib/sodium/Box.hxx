// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "BoxKey.hxx"

#include <sodium/crypto_box.h>

static inline void
crypto_box_keypair(CryptoBoxPublicKeyBuffer pk,
		   CryptoBoxSecretKeyBuffer sk) noexcept
{
	crypto_box_keypair(reinterpret_cast<unsigned char *>(pk.data()),
			   reinterpret_cast<unsigned char *>(sk.data()));
}

static inline void
crypto_box_seal(std::byte *c,
		std::span<const std::byte> m,
		CryptoBoxPublicKeyView pk) noexcept
{
	crypto_box_seal(reinterpret_cast<unsigned char *>(c),
			reinterpret_cast<const unsigned char *>(m.data()),
			m.size(),
			reinterpret_cast<const unsigned char *>(pk.data()));
}

[[nodiscard]]
static inline int
crypto_box_seal_open(std::byte *m,
		     std::span<const std::byte> c,
		     CryptoBoxPublicKeyView pk,
		     CryptoBoxSecretKeyView sk) noexcept
{
	return crypto_box_seal_open(reinterpret_cast<unsigned char *>(m),
				    reinterpret_cast<const unsigned char *>(c.data()),
				    c.size(),
				    reinterpret_cast<const unsigned char *>(pk.data()),
				    reinterpret_cast<const unsigned char *>(sk.data()));
}
