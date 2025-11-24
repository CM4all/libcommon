// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "CharUtil.hxx" // for IsLowerHexDigit()
#include "StringVerify.hxx" // for CheckChars()

static constexpr std::size_t UUID_SIZE = 16;
static constexpr std::size_t UUID_HEX_DIGITS = UUID_SIZE * 2;
static constexpr std::size_t UUID_STRING_LENGTH = UUID_HEX_DIGITS + 4;

/**
 * Is this a well-formed UUID? (Only lower-case hex digits accepted)
 *
 * @see RFC 9562
 */
static constexpr bool
IsLowerUuidString(std::string_view s) noexcept
{
	return s.size() == UUID_STRING_LENGTH &&
		CheckChars(s.substr(0, 8), IsLowerHexDigit) &&
		s[8] == '-' &&
		CheckChars(s.substr(9, 4), IsLowerHexDigit) &&
		s[13] == '-' &&
		CheckChars(s.substr(14, 4), IsLowerHexDigit) &&
		s[18] == '-' &&
		CheckChars(s.substr(19, 4), IsLowerHexDigit) &&
		s[23] == '-' &&
		CheckChars(s.substr(24, 12), IsLowerHexDigit);
}
