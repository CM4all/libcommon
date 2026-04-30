// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <algorithm> // for std::copy(), std::sort()
#include <array>
#include <span>

/**
 * Return a sorted array containing all items in the span.
 *
 * This function is designed to run at compile time
 * (constexpr/constinit).
 */
template<typename T, std::size_t N>
requires(N != std::dynamic_extent)
constexpr auto
SortedArray(const std::span<T, N> src) noexcept
{
	using U = std::decay_t<T>;

	std::array<U, N> dest;
	std::copy(src.begin(), src.end(), dest.begin());
	std::sort(dest.begin(), dest.end());
	return dest;
}
