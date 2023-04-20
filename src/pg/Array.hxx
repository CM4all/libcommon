// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Small utilities for PostgreSQL clients.
 */

#pragma once

#include <forward_list>
#include <string>

namespace Pg {

/**
 * Throws std::invalid_argument on syntax error.
 */
std::forward_list<std::string>
DecodeArray(const char *p);

template<typename L>
requires std::convertible_to<typename L::value_type, std::string_view> && std::forward_iterator<typename L::const_iterator>
std::string
EncodeArray(const L &src) noexcept
{
	if (std::empty(src))
		return "{}";

	std::string dest("{");

	bool first = true;
	for (const std::string_view i : src) {
		if (first)
			first = false;
		else
			dest.push_back(',');

		dest.push_back('"');

		for (const auto ch : i) {
			if (ch == '\\' || ch == '"')
				dest.push_back('\\');
			dest.push_back(ch);
		}

		dest.push_back('"');
	}

	dest.push_back('}');
	return dest;
}

} /* namespace Pg */
