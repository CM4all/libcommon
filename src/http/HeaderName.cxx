/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
