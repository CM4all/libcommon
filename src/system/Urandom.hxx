// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <array>
#include <cstddef>
#include <span>

/**
 * Generate some pseudo-random data, and block until at least one byte
 * has been generated.  Throws if no data at all could be obtained.
 *
 * @return the number of bytes filled with random data
 */
[[nodiscard]]
std::size_t
UrandomRead(std::span<std::byte> dest);

/**
 * Fill the given buffer with pseudo-random data.  May block.  Throws on error.
 */
void
UrandomFill(std::span<std::byte> dest);

/**
 * Return an array filled with pseudo-random data.  May block.  Throws
 * on error.
 */
template<std::size_t size>
std::array<std::byte, size>
UrandomArray()
{
	std::array<std::byte, size> array;
	UrandomFill(array);
	return array;
}
