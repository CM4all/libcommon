// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Reader.hxx"

#include <cstdint>

/**
 * A #Reader implementation that limits the size of another #Reader.
 * When there is more data than the limit, it throws an exception.
 */
class SizeLimitReader : public Reader {
	Reader &next;

	uint_least64_t remaining;

public:
	SizeLimitReader(Reader &_next, uint_least64_t _limit) noexcept
		:next(_next), remaining(_limit) {}

	std::size_t Read(std::span<std::byte> dest) override;
};
