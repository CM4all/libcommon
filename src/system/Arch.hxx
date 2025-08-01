// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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
 * Convert the #Arch to a string.
 *
 * Returns nullptr as a special case for #NONE (because there is no
 * string representation of this special value).
 */
constexpr std::string_view
ToString(Arch arch) noexcept
{
	using std::string_view_literals::operator""sv;

	switch (arch) {
	case Arch::NONE:
		break;

	case Arch::AMD64:
		return "amd64"sv;

	case Arch::ARM64:
		return "arm64"sv;
	}

	return {};
}

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
