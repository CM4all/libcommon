// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "List.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

static std::string_view
http_trim(std::string_view s) noexcept
{
	/* trim whitespace */

	s = Strip(s);

	/* remove quotes from quoted-string */

	if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
		s.remove_prefix(1);
		s.remove_suffix(1);
	}

	/* return */

	return s;
}

static bool
http_equals(std::string_view a, std::string_view b) noexcept
{
	return http_trim(a) == http_trim(b);
}

bool
http_list_contains(std::string_view list,
		   const std::string_view item) noexcept
{
	while (!list.empty()) {
		/* XXX what if the comma is within an quoted-string? */
		auto c = Split(list, ',');
		if (http_equals(c.first, item))
			return true;

		list = c.second;
	}

	return false;
}

static bool
http_equals_i(std::string_view a, std::string_view b) noexcept
{
	return StringIsEqualIgnoreCase(http_trim(a), b);
}

bool
http_list_contains_i(std::string_view list,
		     const std::string_view item) noexcept
{
	while (!list.empty()) {
		/* XXX what if the comma is within an quoted-string? */
		auto c = Split(list, ',');
		if (http_equals_i(c.first, item))
			return true;

		list = c.second;
	}

	return false;
}
