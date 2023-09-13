// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "MemberPointer.hxx"
#include "CopyConst.hxx"

#include <utility>

/**
 * An adapter for an existing iterator class which returns a member of
 * the iterator's type.
 *
 * TODO: implement operator+=() operator+() operator-() for
 * RandomAccessIterator
 */
template<typename I, auto member>
class MemberIteratorAdapter : I
{
public:
	using typename I::iterator_category;
	using value_type = MemberPointerType<decltype(member)>;
	using maybe_const_value_type = CopyConst<value_type,
						 std::remove_reference_t<typename I::reference>>;
	using typename I::difference_type;
	using pointer = maybe_const_value_type *;
	using reference = maybe_const_value_type &;

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

	friend auto operator<=>(const MemberIteratorAdapter &,
				const MemberIteratorAdapter &) noexcept = default;

	constexpr reference operator*() const {
		return I::operator*().*member;
	}

	constexpr pointer operator->() const {
		return &operator*();
	}
};
