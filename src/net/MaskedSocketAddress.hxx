// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "AllocatedSocketAddress.hxx"

#include <utility>

class MaskedSocketAddress {
	AllocatedSocketAddress address;
	unsigned prefix_length;

public:
	template<typename A>
	MaskedSocketAddress(A &&_address, unsigned _prefix_length)
		:address(std::forward<A>(_address)), prefix_length(_prefix_length) {}

	/**
	 * Parse a string with a numeric address and an optional prefix
	 * separated with a slash.
	 *
	 * Throws std::runtime_error on error.
	 */
	explicit MaskedSocketAddress(const char *s);

	[[gnu::pure]]
	bool Matches(SocketAddress other) const noexcept;
};
