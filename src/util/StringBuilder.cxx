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

template class BasicStringBuilder<char>;
