// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/CharUtil.hxx"
#include "util/StringVerify.hxx"

/**
 * @see RFC 7230 3.2
 */
static constexpr bool
IsValidHttpHeaderValueChar(char ch) noexcept
{
	/* the IsNonPrintableASCII() function disallows only
	   0x00..0x1f; it allows all printable ASCII characters and
	   also non-ASCII characters (e.g. UTF-8); the htab is allowed
	   in HTTP header values, so we have to mention it explicitly
	   here */

	return !IsNonPrintableASCII(ch) || ch == '\t';
}

/**
 * @see RFC 7230 3.2
 */
static constexpr bool
IsValidHttpHeaderValue(const auto &value) noexcept
{
	/* space and tab are only allowed in the middle of the string,
	   but this function ignores that for simplicity */
	return CheckChars(value, IsValidHttpHeaderValueChar);
}
