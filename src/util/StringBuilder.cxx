// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "StringBuilder.hxx"

#include <algorithm>

template<typename T>
void
BasicStringBuilder<T>::UnsafeAppend(std::basic_string_view<T> src) noexcept
{
	p = std::copy(src.begin(), src.end(), p);
	*p = SENTINEL;
}

template<typename T>
void
BasicStringBuilder<T>::UnsafeFill(T ch, std::size_t n) noexcept
{
	p = std::fill_n(p, n, ch);
	*p = SENTINEL;
}

template class BasicStringBuilder<char>;
