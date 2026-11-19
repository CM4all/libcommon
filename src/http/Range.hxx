// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstdint>

struct HttpRangeRequest {
	enum class Type {
		NONE,
		VALID,
		INVALID,
	} type = Type::NONE;

	uint_least64_t skip = 0, size;

	explicit constexpr HttpRangeRequest(uint_least64_t _size) noexcept
		:size(_size) {}

	/**
	 * Parse a "Range" request header.
	 */
	void ParseRangeHeader(const char *p) noexcept;
};
