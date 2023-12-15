// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "CharUtil.hxx"

#include <array>
#include <concepts>
#include <cstdint>
#include <string_view>

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
template<std::unsigned_integral T>
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
	auto &output = (uint8_t &)_output;
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
