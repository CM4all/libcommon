// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <string_view>

/**
 * A key for #MapStock and #MultiStock.  It contains a string value
 * and a precalculated hash.
 */
struct StockKey {
	std::size_t hash;
	std::string_view value;

	/**
	 * Construct a key with the default hash function.
	 */
	explicit StockKey(std::string_view _value) noexcept;

	explicit constexpr StockKey(std::string_view _value, std::size_t _hash) noexcept
		:hash(_hash), value(_value) {}

	constexpr bool operator==(const StockKey &) const noexcept = default;
};
