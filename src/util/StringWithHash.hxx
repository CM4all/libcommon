// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

	constexpr bool operator==(const StringWithHash &) const noexcept = default;
};
