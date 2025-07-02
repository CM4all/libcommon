// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "StringSplit.hxx"

#include <array>

/**
 * A variant of Split() that can do multiple splits.
 *
 * @param N the number of splits (constant expression)
 * @return a std::tuple with N+1 string_views
 */
template<std::size_t N, typename T>
constexpr auto
StringMultiSplit(std::basic_string_view<T> haystack, const T ch) noexcept
{
	std::array<std::string_view, N + 1> result;

	for (std::size_t i = 0; i < N; ++i) {
		const auto [a, b] = Split(haystack, ch);
		result[i] = a;
		haystack = b;
	}

	result[N] = haystack;

	return result;
}
