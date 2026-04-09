// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "HeaderName.hxx"
#include "util/StringAPI.hxx"

#include <cassert>

static constexpr bool
http_header_name_char_valid(char ch) noexcept
{
	return (signed char)ch > 0x20 && ch != ':';
}

bool
http_header_name_valid(const char *name) noexcept
{
	do {
		if (!http_header_name_char_valid(*name))
			return false;
	} while (*++name != 0);

	return true;
}

bool
http_header_name_valid(std::string_view name) noexcept
{
	if (name.empty())
		return false;

	for (char ch : name)
		if (!http_header_name_char_valid(ch))
			return false;

	return true;
}

bool
http_header_is_hop_by_hop(const char *name) noexcept
{
	assert(name != nullptr);

	switch (*name) {
	case 'c':
		return StringIsEqual(name, "connection") ||
			StringIsEqual(name, "content-length");

	case 'e':
		/* RFC 2616 14.20 */
		return StringIsEqual(name, "expect");

	case 'k':
		return StringIsEqual(name, "keep-alive");

	case 'p':
		return StringIsEqual(name, "proxy-authenticate") ||
			StringIsEqual(name, "proxy-authorization");

	case 't':
		return StringIsEqual(name, "te") ||
			/* typo in RFC 2616? */
			StringIsEqual(name, "trailer") ||
			StringIsEqual(name, "trailers") ||
			StringIsEqual(name, "transfer-encoding");

	case 'u':
		return StringIsEqual(name, "upgrade");

	default:
		return false;
	}
}
