// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "BoxKey.hxx"

#include <sodium/crypto_box.h>

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
