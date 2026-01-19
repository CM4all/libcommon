// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "XChaCha20Types.hxx"

#include <sodium/crypto_stream_xchacha20.h>

#include <cstddef>
#include <span>

static_assert(sizeof(XChaCha20Key) == crypto_stream_xchacha20_KEYBYTES);
static_assert(sizeof(XChaCha20Nonce) == crypto_stream_xchacha20_NONCEBYTES);

static void
crypto_stream_xchacha20_xor(std::byte *c, std::span<const std::byte> m,
			    XChaCha20NonceView n,
			    XChaCha20KeyView k) noexcept
{
	crypto_stream_xchacha20_xor(reinterpret_cast<unsigned char *>(c),
				    reinterpret_cast<const unsigned char *>(m.data()), m.size(),
				    reinterpret_cast<const unsigned char *>(n.data()),
				    reinterpret_cast<const unsigned char *>(k.data()));
}

static void
crypto_stream_xchacha20_xor_ic(std::byte *c, std::span<const std::byte> m,
			       XChaCha20NonceView n,
			       uint64_t ic,
			       XChaCha20KeyView k) noexcept
{
	crypto_stream_xchacha20_xor_ic(reinterpret_cast<unsigned char *>(c),
				       reinterpret_cast<const unsigned char *>(m.data()), m.size(),
				       reinterpret_cast<const unsigned char *>(n.data()),
				       ic,
				       reinterpret_cast<const unsigned char *>(k.data()));
}
