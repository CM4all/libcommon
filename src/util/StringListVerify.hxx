// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "IterableSplitString.hxx"

#include <string_view>

template <typename F>
[[gnu::pure]]
bool
IsNonEmptyListOf(std::string_view s, char separator, F &&f) noexcept
{
	if (s.empty())
	    return false;

	for (const std::string_view i : IterableSplitString(s, separator))
		if (!f(i))
			return false;

	return true;
}
