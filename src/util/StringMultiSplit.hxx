// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "StringSplit.hxx"

#include <tuple>

template<typename T>
constexpr std::basic_string_view<T>
_SplitInPlace(std::basic_string_view<T> &haystack, const T ch) noexcept
{
	const auto [a, b] = Split(haystack, ch);
	haystack = b;
	return a;
}

template<typename T, std::size_t... Is>
constexpr auto
_StringMultiSplit(std::basic_string_view<T> haystack, const T ch, std::index_sequence<Is...>) noexcept
{
	if constexpr (sizeof...(Is) == 0) {
		// suppress -Wunused for a corner case
		(void)ch;
	}

	return std::make_tuple((static_cast<void>(Is),
				_SplitInPlace(haystack, ch))...,
			       haystack);
}

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
	return _StringMultiSplit<T>(haystack, ch, std::make_index_sequence<N>{});
}
