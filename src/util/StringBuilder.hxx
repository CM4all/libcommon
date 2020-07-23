/*
 * Copyright 2015-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef STRING_BUILDER_HXX
#define STRING_BUILDER_HXX

#include "ConstBuffer.hxx"
#include "WritableBuffer.hxx"
#include "Compiler.h"

#include <cstddef>

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
	constexpr explicit BasicStringBuilder(WritableBuffer<value_type> b) noexcept
		:p(b.begin()), end(b.end()) {}

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

	WritableBuffer<value_type> Write() const noexcept {
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

	void Append(ConstBuffer<T> src) {
		Append(src.data, src.size);
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

#endif
