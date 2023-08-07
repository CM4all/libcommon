// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <nettle/sha1.h>

#include <array>
#include <cstddef> // for std::byte
#include <span>

namespace Nettle {

class SHA1Ctx {
	struct sha1_ctx ctx;

public:
	SHA1Ctx() noexcept {
		sha1_init(&ctx);
	}

	auto &Update(std::span<const std::byte> src) noexcept {
		sha1_update(&ctx, src.size(),
			    reinterpret_cast<const uint8_t *>(src.data()));
		return *this;
	}

	void Digest(std::span<std::byte> dest) noexcept {
		sha1_digest(&ctx, dest.size(),
			    reinterpret_cast<uint8_t *>(dest.data()));
	}

	template<std::size_t size=SHA1_DIGEST_SIZE>
	auto Digest() noexcept {
		std::array<std::byte, size> digest;
		Digest(std::span{digest});
		return digest;
	}
};

} // namespace Nettle
