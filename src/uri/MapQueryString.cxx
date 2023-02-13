// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MapQueryString.hxx"
#include "Unescape.hxx"
#include "util/AllocatedArray.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringSplit.hxx"

#include <stdexcept>

/**
 * Unescape a form value according to RFC 1866 8.2.1.  This uses
 * UriUnescape(), but also supports space encoded as "+".
 */
static char *
FormUnescape(char *dest, std::string_view src) noexcept
{
	while (true) {
		const auto [segment, rest] = Split(src, '+');
		dest = UriUnescape(dest, segment);

		if (rest.data() == nullptr)
			return dest;

		*dest++ = ' ';

		src = rest;
	}
}

std::multimap<std::string, std::string, std::less<>>
MapQueryString(std::string_view src)
{
	std::multimap<std::string, std::string, std::less<>> m;

	AllocatedArray<char> unescape_buffer(256);

	for (const std::string_view i : IterableSplitString(src, '&')) {
		const auto [name, escaped_value] = Split(i, '=');
		if (name.empty())
			continue;

		unescape_buffer.GrowDiscard(escaped_value.size());
		char *value = unescape_buffer.data();
		const char *end_value = FormUnescape(value, escaped_value);
		if (end_value == nullptr)
			throw std::invalid_argument{"Malformed URI escape"};

		m.emplace(name, std::string_view{value, std::size_t(end_value - value)});
	}

	return m;
}
