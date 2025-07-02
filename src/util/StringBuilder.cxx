// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "StringBuilder.hxx"
#include "StringAPI.hxx"

#include <algorithm>

template<typename T>
void
BasicStringBuilder<T>::Append(const_pointer src)
{
	Append(src, StringLength(src));
}

template<typename T>
void
BasicStringBuilder<T>::Append(const_pointer src, size_t length)
{
	CheckAppend(length);
	p = std::copy_n(src, length, p);
	*p = SENTINEL;
}

template class BasicStringBuilder<char>;
