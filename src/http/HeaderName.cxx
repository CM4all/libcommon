// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "HeaderName.hxx"
#include "Token.hxx"
#include "util/StringAPI.hxx"
#include "util/StringVerify.hxx"

#include <cassert>

bool
http_header_name_valid(const char *name) noexcept
{
	return IsHttpToken(name);
}

bool
http_header_name_valid(std::string_view name) noexcept
{
	return IsHttpToken(name);
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

	case 'h':
		/* this one belongs to the "Upgrade: h2c" request, and
		   it is hop-by-hop (RFC 7540 3.2) even though it is
		   not in the RFC 9110 list */
		return StringIsEqual(name, "http2-settings");

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
