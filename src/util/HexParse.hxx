/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "CharUtil.hxx"

#include <array>
#include <string_view>
#include <type_traits>

constexpr int
ParseHexDigit(char ch) noexcept
{
	if (IsDigitASCII(ch))
		return ch - '0';
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 0xa;
	else if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 0xa;
	else
		return -1;
}

constexpr int
ParseLowerHexDigit(char ch) noexcept
{
	if (IsDigitASCII(ch))
		return ch - '0';
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 0xa;
	else
		return -1;
}

/**
 * Parse the hex digits of a fixed-length string to the given integer
 * reference.
 *
 * @return the end of the parsed string on success, nullptr on error
 */
template<typename T,
	 typename=std::enable_if_t<std::is_integral_v<T> &&
				   std::is_unsigned_v<T>>>
const char *
ParseLowerHexFixed(const char *input, T &output) noexcept
{
	T value{};

	for (std::size_t j = 0; j < sizeof(T) * 2; ++j) {
		int digit = ParseLowerHexDigit(*input++);
		if (digit < 0)
			return nullptr;

		value = (value << 4) | digit;
	}

	output = value;
	return input;
}

inline const char *
ParseLowerHexFixed(const char *input, std::byte &_output) noexcept
{
	auto &output = (std::uint8_t &)_output;
	return ParseLowerHexFixed(input, output);
}

/**
 * Parse the hex digits of a fixed-length string to the given integer
 * array.
 *
 * @return the end of the parsed string on success, nullptr on error
 */
template<typename T, std::size_t size>
const char *
ParseLowerHexFixed(const char *input, std::array<T, size> &output) noexcept
{
	for (auto &i : output) {
		input = ParseLowerHexFixed(input, i);
		if (input == nullptr)
			return nullptr;
	}

	return input;
}

template<typename T>
bool
ParseLowerHexFixed(std::string_view input, T &output) noexcept
{
	return input.size() == sizeof(output) * 2 &&
		ParseLowerHexFixed(input.data(), output) != nullptr;
}
