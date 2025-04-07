// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>
#include <string_view>

/**
 * An enum which identifies a CPU architecture.
 */
enum class Arch : uint_least8_t {
	/**
	 * Special value for "none", "unspecified", "unknown".
	 */
	NONE,

	/**
	 * AMD64, aka "x86-64".
	 */
	AMD64,

	/**
	 * ARM64, aka "aarch64".
	 */
	ARM64,
};

/**
 * Parse a string to an #Arch.  Returns #NONE if the string is not
 * recognized.
 */
constexpr Arch
ParseArch(std::string_view s) noexcept
{
	using std::string_view_literals::operator""sv;

	if (s == "amd64"sv)
		return Arch::AMD64;
	else if (s == "arm64"sv)
		return Arch::ARM64;
	else
		return Arch::NONE;
}
