// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "ChaCha20Types.hxx"

#include <sodium/crypto_stream_chacha20.h>

#include <cstddef>
#include <span>

static_assert(sizeof(ChaCha20Key) == crypto_stream_chacha20_KEYBYTES);
static_assert(sizeof(ChaCha20NonceView) == crypto_stream_chacha20_NONCEBYTES);

static void
crypto_stream_chacha20_xor(std::byte *c, std::span<const std::byte> m,
			   ChaCha20NonceView n,
			   ChaCha20KeyView k) noexcept
{
	crypto_stream_chacha20_xor(reinterpret_cast<unsigned char *>(c),
				   reinterpret_cast<const unsigned char *>(m.data()), m.size(),
				   reinterpret_cast<const unsigned char *>(n.data()),
				   reinterpret_cast<const unsigned char *>(k.data()));
}

static void
crypto_stream_chacha20_xor_ic(std::byte *c, std::span<const std::byte> m,
			      ChaCha20NonceView n,
			      uint64_t ic,
			      ChaCha20KeyView k) noexcept
{
	crypto_stream_chacha20_xor_ic(reinterpret_cast<unsigned char *>(c),
				      reinterpret_cast<const unsigned char *>(m.data()), m.size(),
				      reinterpret_cast<const unsigned char *>(n.data()),
				      ic,
				      reinterpret_cast<const unsigned char *>(k.data()));
}
