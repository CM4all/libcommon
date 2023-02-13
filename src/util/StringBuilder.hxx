// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Compiler.h"

#include <cstddef>
#include <span>

/**
 * Fills a string buffer incrementally by appending more data to the
 * end, truncating the string if the buffer is full.
 */
template<typename T=char>
class BasicStringBuilder {
	using value_type = T;
	using pointer = T *;
	using const_pointer = const T *;
	using size_type = std::size_t;

	pointer p;
	const pointer end;

	static constexpr value_type SENTINEL = '\0';

public:
	constexpr explicit BasicStringBuilder(std::span<value_type> b) noexcept
		:p(b.data()), end(p + b.size()) {}

	constexpr BasicStringBuilder(pointer _p, pointer _end) noexcept
		:p(_p), end(_end) {}

	constexpr BasicStringBuilder(pointer _p, size_type size) noexcept
		:p(_p), end(p + size) {}

	constexpr pointer GetTail() const noexcept {
		return p;
	}

	constexpr size_type GetRemainingSize() const noexcept {
		return end - p;
	}

	constexpr bool IsFull() const noexcept {
		return p >= end - 1;
	}

	std::span<value_type> Write() const noexcept {
		return {p, end};
	}

	void Extend(size_type length) noexcept {
		p += length;
	}

	/**
	 * This class gets thrown when the buffer would overflow by an
	 * operation.  The buffer is then in an undefined state.
	 */
	class Overflow {};

	constexpr bool CanAppend(size_type length) const noexcept {
		return p + length < end;
	}

	void CheckAppend(size_type length) const {
		if (!CanAppend(length))
			throw Overflow();
	}

	void Append(T ch) {
		CheckAppend(1);

		*p++ = ch;
		*p = SENTINEL;
	}

	void Append(const_pointer src);
	void Append(const_pointer src, size_t length);

	void Append(std::span<const T> src) {
		Append(src.data(), src.size());
	}

#ifndef __clang__
	/* clang thinks the "format argument not a string type"
	   because it depends on a template argument */
	gcc_printf(2, 3)
#endif
	void Format(const_pointer fmt, ...);
};

class StringBuilder : public BasicStringBuilder<char> {
public:
	using BasicStringBuilder<char>::BasicStringBuilder;
};
