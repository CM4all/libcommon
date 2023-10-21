// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <sodium/crypto_stream_chacha20.h>

#include <cstddef>
#include <span>

static void
crypto_stream_chacha20_xor(std::byte *c, std::span<const std::byte> m,
			   std::span<const std::byte, crypto_stream_chacha20_NONCEBYTES> n,
			   std::span<const std::byte, crypto_stream_chacha20_KEYBYTES> k) noexcept
{
	crypto_stream_chacha20_xor(reinterpret_cast<unsigned char *>(c),
				   reinterpret_cast<const unsigned char *>(m.data()), m.size(),
				   reinterpret_cast<const unsigned char *>(n.data()),
				   reinterpret_cast<const unsigned char *>(k.data()));
}

static void
crypto_stream_chacha20_xor_ic(std::byte *c, std::span<const std::byte> m,
			      std::span<const std::byte, crypto_stream_chacha20_NONCEBYTES> n,
			      uint64_t ic,
			      std::span<const std::byte, crypto_stream_chacha20_KEYBYTES> k) noexcept
{
	crypto_stream_chacha20_xor_ic(reinterpret_cast<unsigned char *>(c),
				      reinterpret_cast<const unsigned char *>(m.data()), m.size(),
				      reinterpret_cast<const unsigned char *>(n.data()),
				      ic,
				      reinterpret_cast<const unsigned char *>(k.data()));
}
