// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "AllocatedSocketAddress.hxx"

#include <cstdint>
#include <utility>

class MaskedSocketAddress {
	AllocatedSocketAddress address;
	uint_least8_t prefix_length;

public:
	template<typename A>
	MaskedSocketAddress(A &&_address, uint_least8_t _prefix_length) noexcept
		:address(std::forward<A>(_address)), prefix_length(_prefix_length) {}

	/**
	 * Parse a string with a numeric address and an optional prefix
	 * separated with a slash.
	 *
	 * Throws std::runtime_error on error.
	 */
	explicit MaskedSocketAddress(const char *s);

	[[gnu::pure]]
	static uint_least8_t MaximumPrefixLength(const SocketAddress address) noexcept;

	[[gnu::pure]]
	static bool IsValidPrefixLength(SocketAddress address,
					uint_least8_t prefix_length) noexcept;

	[[gnu::pure]]
	static bool Matches(SocketAddress address, uint_least8_t prefix_length,
			    SocketAddress other) noexcept;

	[[gnu::pure]]
	bool Matches(SocketAddress other) const noexcept {
		return Matches(address, prefix_length, other);
	}
};
