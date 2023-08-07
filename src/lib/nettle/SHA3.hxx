// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <nettle/sha3.h>

#include <array>
#include <cstddef> // for std::byte
#include <span>

namespace Nettle {

class SHA3_256Ctx {
	struct sha3_256_ctx ctx;

public:
	SHA3_256Ctx() noexcept {
		sha3_256_init(&ctx);
	}

	auto &Update(std::span<const std::byte> src) noexcept {
		sha3_256_update(&ctx, src.size(),
			    reinterpret_cast<const uint8_t *>(src.data()));
		return *this;
	}

	void Digest(std::span<std::byte> dest) noexcept {
		sha3_256_digest(&ctx, dest.size(),
				reinterpret_cast<uint8_t *>(dest.data()));
	}

	template<std::size_t size=SHA3_256_DIGEST_SIZE>
	auto Digest() noexcept {
		std::array<std::byte, size> digest;
		Digest(std::span{digest});
		return digest;
	}
};

} // namespace Nettle
