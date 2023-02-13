// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Unescape.hxx"
#include "util/HexParse.hxx"

#include <algorithm>

char *
UriUnescape(char *dest, std::string_view _src, char escape_char) noexcept
{
	auto src = _src.begin();
	const auto end = _src.end();

	while (true) {
		auto p = std::find(src, end, escape_char);
		dest = std::copy(src, p, dest);

		if (p == end)
			break;

		if (p >= end - 2)
			/* percent sign at the end of string */
			return nullptr;

		const int digit1 = ParseHexDigit(p[1]);
		const int digit2 = ParseHexDigit(p[2]);
		if (digit1 == -1 || digit2 == -1)
			/* invalid hex digits */
			return nullptr;

		const char ch = (char)((digit1 << 4) | digit2);
		if (ch == 0)
			/* no %00 hack allowed! */
			return nullptr;

		*dest++ = ch;
		src = p + 3;
	}

	return dest;
}
