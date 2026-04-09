// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/CharUtil.hxx"
#include "util/StringVerify.hxx"

/**
 * @see RFC 7230 3.2.6
 */
constexpr bool
IsHttpTokenChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) ||
		ch == '!' || ch == '#' || ch == '$' || ch == '%' ||
		ch == '&' || ch == '\'' || ch == '*' || ch == '+' ||
		ch == '-' || ch == '.' || ch == '^' || ch == '_' ||
		ch == '`' || ch == '|' || ch == '~';
}

/**
 * @see RFC 7230 3.2.6
 */
constexpr bool
IsHttpToken(const auto &s) noexcept
{
	return CheckCharsNonEmpty(s, IsHttpTokenChar);
}
