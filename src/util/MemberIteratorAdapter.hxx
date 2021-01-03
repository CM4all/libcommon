/*
 * Copyright 2017-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <iterator>

/**
 * An adapter for an existing iterator class which returns a member of
 * the iterator's type.
 *
 * TODO: implement operator+=() operator+() operator-() for
 * RandomAccessIterator
 */
template<typename I, typename C, typename M, M C::*member>
class MemberIteratorAdapter : I
{
public:
	using typename I::iterator_category;
	typedef M value_type;
	using typename I::difference_type;
	using pointer = M *;
	using reference = M &;

	template<typename T>
	constexpr MemberIteratorAdapter(T &&t) noexcept
		:I(std::forward<T>(t)) {}

	MemberIteratorAdapter &operator++() {
		I::operator++();
		return *this;
	}

	MemberIteratorAdapter operator++(int) {
		auto result = *this;
		++(*this);
		return result;
	}

	MemberIteratorAdapter &operator--() {
		I::operator--();
		return *this;
	}

	MemberIteratorAdapter operator--(int) {
		auto result = *this;
		--(*this);
		return result;
	}

	constexpr bool operator==(const MemberIteratorAdapter &other) const {
		return static_cast<const I &>(*this) == static_cast<const I &>(other);
	}

	constexpr bool operator!=(const MemberIteratorAdapter &other) const {
		return !(*this == other);
	}

	constexpr reference operator*() const {
		return I::operator*().*member;
	}

	constexpr pointer operator->() const {
		return &operator*();
	}
};
