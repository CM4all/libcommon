// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <sodium/crypto_onetimeauth_poly1305.h>

#include <cstddef>
#include <span>

static void
crypto_onetimeauth_poly1305(std::span<std::byte, crypto_onetimeauth_poly1305_BYTES> out,
			    std::span<const std::byte> in,
			    std::span<const std::byte, crypto_onetimeauth_poly1305_KEYBYTES> k) noexcept
{
	crypto_onetimeauth_poly1305(reinterpret_cast<unsigned char *>(out.data()),
				    reinterpret_cast<const unsigned char *>(in.data()), in.size(),
				    reinterpret_cast<const unsigned char *>(k.data()));
}

[[nodiscard]] [[gnu::pure]]
static bool
crypto_onetimeauth_poly1305_verify(std::span<const std::byte, crypto_onetimeauth_poly1305_BYTES> h,
				   std::span<const std::byte> in,
				   std::span<const std::byte, crypto_onetimeauth_poly1305_KEYBYTES> k) noexcept
{
	return crypto_onetimeauth_poly1305_verify(reinterpret_cast<const unsigned char *>(h.data()),
						  reinterpret_cast<const unsigned char *>(in.data()), in.size(),
						  reinterpret_cast<const unsigned char *>(k.data())) == 0;
}
