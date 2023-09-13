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
class MemberIteratorAdapter
{
	I i;

public:
	using iterator_category = typename I::iterator_category;
	using value_type = MemberPointerType<decltype(member)>;
	using maybe_const_value_type = CopyConst<value_type,
						 std::remove_reference_t<typename I::reference>>;
	using difference_type = typename I::difference_type;
	using pointer = maybe_const_value_type *;
	using reference = maybe_const_value_type &;

	template<typename T>
	constexpr MemberIteratorAdapter(T &&t) noexcept
		:i(std::forward<T>(t)) {}

	constexpr MemberIteratorAdapter(const MemberIteratorAdapter &) noexcept = default;
	constexpr MemberIteratorAdapter(MemberIteratorAdapter &) noexcept = default;

	constexpr MemberIteratorAdapter &operator++() noexcept {
		++i;
		return *this;
	}

	constexpr MemberIteratorAdapter operator++(int) noexcept {
		return i++;
	}

	constexpr MemberIteratorAdapter &operator--() noexcept {
		--i;
		return *this;
	}

	constexpr MemberIteratorAdapter operator--(int) noexcept {
		return i--;
	}

	friend auto operator<=>(const MemberIteratorAdapter &,
				const MemberIteratorAdapter &) noexcept = default;

	constexpr reference operator*() const noexcept {
		return (*i).*member;
	}

	constexpr pointer operator->() const noexcept {
		return &operator*();
	}
};
