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
	return IsPrintableASCII(ch) || ch == '\t';
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
