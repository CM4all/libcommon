// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstddef>
#include <string_view>

/**
 * A string (view) and a hash associated with it.  This is designed to
 * be used as key for hash tables (e.g. #IntrusiveHashSet).
 *
 * This class does not own the memory the #value points to, as it only
 * contains a std::string_view.
 */
struct StringWithHash {
	std::size_t hash;
	std::string_view value;

	/**
	 * Construct a key with the default hash function.
	 */
	explicit StringWithHash(std::string_view _value) noexcept;

	explicit constexpr StringWithHash(std::string_view _value, std::size_t _hash) noexcept
		:hash(_hash), value(_value) {}

	/**
	 * Construct a "nulled" instance, i.e. one where IsNull()
	 * returns true.
	 */
	explicit constexpr StringWithHash(std::nullptr_t) noexcept
		:hash(0), value() {}

	constexpr bool IsNull() const noexcept {
		return value.data() == nullptr;
	}

	constexpr bool operator==(const StringWithHash &) const noexcept = default;
};

template<>
struct std::hash<StringWithHash> {
	constexpr std::size_t operator()(const StringWithHash &s) const noexcept {
		return s.hash;
	}
};
