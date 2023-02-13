// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef HTTP_RANGE_HXX
#define HTTP_RANGE_HXX

#include <stdint.h>

struct HttpRangeRequest {
	enum class Type {
		NONE,
		VALID,
		INVALID,
	} type = Type::NONE;

	uint64_t skip = 0, size;

	explicit constexpr HttpRangeRequest(uint64_t _size) noexcept
		:size(_size) {}

	/**
	 * Parse a "Range" request header.
	 */
	void ParseRangeHeader(const char *p) noexcept;
};

#endif
