// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <array>
#include <functional>

/**
 * A hashtable key based on a libsodium "generichash" (Blake2B) hash.
 * This can be used as a small and fixed-size hashtable key instead of
 * using a larger complex or variable-sized data structure (e.g. a
 * std::string) when this fixed-size hash is reliable
 * (collision-proof) enough.
 */
struct HashKey {
	/**
	 * This should be the same as
	 * crypto_generichash_blake2b_BYTES_MIN, but we don't include
	 * the libsodium header here to keep header bloat low.
	 */
	static constexpr std::size_t SIZE = 16;

	/**
	 * We're storing std::size_t elements because that's what
	 * std::hash::operator() is expected to return.  Using
	 * std::size_t internally gives us the best performance
	 * because it defines a proper alignment for this struct.
	 */
	using value_type = std::size_t;

	static_assert(SIZE % sizeof(value_type) == 0);
	static constexpr std::size_t N = 16 / sizeof(value_type);

	std::array<value_type, N> values;

	bool operator==(HashKey other) const noexcept {
		return values == other.values;
	}
};

static_assert(sizeof(HashKey) == HashKey::SIZE);

namespace std {

template<> struct hash<::HashKey> {
	std::size_t operator()(HashKey hk) const noexcept {
		return hk.values.front();
	}
};

}
