// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <sodium/crypto_hash_sha512.h>

#include <array>
#include <cstddef>
#include <span>

using SHA512DigestBuffer = std::array<std::byte, crypto_hash_sha512_BYTES>;
using SHA512DigestView = std::span<const std::byte, crypto_hash_sha512_BYTES>;

class SHA512State {
	crypto_hash_sha512_state state;

public:
	SHA512State() noexcept {
		crypto_hash_sha512_init(&state);
	}

	void Update(std::span<const std::byte> p) noexcept {
		crypto_hash_sha512_update(&state,
					  reinterpret_cast<const unsigned char *>(p.data()),
					  p.size());
	}

	void Final(std::span<std::byte, crypto_hash_sha512_BYTES> out) noexcept {
		crypto_hash_sha512_final(&state,
					 reinterpret_cast<unsigned char *>(out.data()));
	}

	auto Final() noexcept {
		SHA512DigestBuffer out;
		Final(out);
		return out;
	}
};

[[gnu::pure]]
inline auto
SHA512(std::span<const std::byte> src) noexcept
{
	SHA512DigestBuffer out;
	crypto_hash_sha512(reinterpret_cast<unsigned char *>(out.data()),
			   reinterpret_cast<const unsigned char *>(src.data()),
			   src.size());
	return out;
}
