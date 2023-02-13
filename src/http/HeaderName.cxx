// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "HeaderName.hxx"

#include <cassert>

#include <string.h>

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
		return strcmp(name, "connection") == 0 ||
			strcmp(name, "content-length") == 0;

	case 'e':
		/* RFC 2616 14.20 */
		return strcmp(name, "expect") == 0;

	case 'k':
		return strcmp(name, "keep-alive") == 0;

	case 'p':
		return strcmp(name, "proxy-authenticate") == 0 ||
			strcmp(name, "proxy-authorization") == 0;

	case 't':
		return strcmp(name, "te") == 0 ||
			/* typo in RFC 2616? */
			strcmp(name, "trailer") == 0 || strcmp(name, "trailers") == 0 ||
			strcmp(name, "transfer-encoding") == 0;

	case 'u':
		return strcmp(name, "upgrade") == 0;

	default:
		return false;
	}
}
