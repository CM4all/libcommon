// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <algorithm>
#include <string_view>

/**
 * Does the given string consist only of characters allowed by the
 * given function?
 */
template<typename F>
constexpr bool
CheckChars(std::string_view s, F &&f) noexcept
{
	return std::all_of(s.begin(), s.end(), std::forward<F>(f));
}

/**
 * Is the given string non-empty and consists only of characters
 * allowed by the given function?
 */
template<typename F>
constexpr bool
CheckCharsNonEmpty(std::string_view s, F &&f) noexcept
{
	return !s.empty() && CheckChars(s, std::forward<F>(f));
}

template<typename F>
constexpr bool
CheckCharsNonEmpty(const char *s, F &&f) noexcept
{
	do {
		if (!f(*s))
			return false;
	} while (*++s);
	return true;
}
