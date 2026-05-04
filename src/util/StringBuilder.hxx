// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "TooLargeError.hxx"

#include <cstddef>
#include <span>
#include <string_view>

/**
 * Fills a string buffer incrementally by appending more data to the
 * end.
 *
 * Append methods throw #TooLargeError when the buffer would overflow
 * by an operation.  The buffer is then in an undefined state.
 */
template<typename T=char>
class BasicStringBuilder {
	using value_type = T;
	using pointer = T *;
	using const_pointer = const T *;
	using size_type = std::size_t;

	pointer p;
	const pointer end;

public:
	[[nodiscard]]
	explicit constexpr BasicStringBuilder(std::span<value_type> b) noexcept
		:p(b.data()), end(p + b.size()) {}

	constexpr pointer GetTail() const noexcept {
		return p;
	}

	constexpr size_type GetRemainingSize() const noexcept {
		return end - p;
	}

	constexpr bool IsFull() const noexcept {
		return p >= end;
	}

	[[nodiscard]]
	constexpr std::span<value_type> Write() const noexcept {
		return {p, end};
	}

	constexpr void Extend(size_type length) noexcept {
		p += length;
	}

	constexpr bool CanAppend(size_type length) const noexcept {
		return p + length <= end;
	}

	constexpr void CheckAppend(size_type length) const {
		if (!CanAppend(length))
			throw TooLargeError{};
	}

	constexpr void UnsafeAppend(T ch) noexcept {
		*p++ = ch;
	}

	[[nodiscard]]
	constexpr bool TryAppend(T ch) noexcept {
		const bool success = CanAppend(1);
		if (success) [[likely]]
			UnsafeAppend(ch);
		return success;
	}

	constexpr void Append(T ch) {
		CheckAppend(1);
		UnsafeAppend(ch);
	}

	/**
	 * Like Append(), but do not check whether there is enough
	 * space in the buffer.
	 */
	void UnsafeAppend(std::basic_string_view<T> src) noexcept;

	[[nodiscard]]
	constexpr bool TryAppend(std::basic_string_view<T> src) noexcept {
		const bool success = CanAppend(src.size());
		if (success) [[likely]]
			UnsafeAppend(src);
		return success;
	}

	void Append(std::basic_string_view<T> src) {
		CheckAppend(src.size());
		UnsafeAppend(src);
	}

	/**
	 * Like Fill(), but do not check whether there is enough space
	 * in the buffer.
	 */
	void UnsafeFill(T ch, std::size_t n) noexcept;

	/**
	 * Append #n copies of the specified characters.
	 *
	 * May throw #TooLargeError.
	 */
	void Fill(T ch, std::size_t n) {
		CheckAppend(n);
		UnsafeFill(ch, n);
	}

	constexpr std::string_view ToStringView(std::span<const value_type> buffer) const noexcept {
		return {buffer.data(), p};
	}
};

class StringBuilder : public BasicStringBuilder<char> {
public:
	using BasicStringBuilder<char>::BasicStringBuilder;
};
