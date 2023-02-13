// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Escape.hxx"
#include "uri/Chars.hxx"
#include "util/AllocatedString.hxx"
#include "util/HexFormat.hxx"
#include "util/SpanCast.hxx"

std::size_t
UriEscape(char *dest, std::string_view src,
	  char escape_char) noexcept
{
	std::size_t dest_length = 0;

	for (std::size_t i = 0; i < src.size(); ++i) {
		if (IsUriUnreservedChar(src[i])) {
			dest[dest_length++] = src[i];
		} else {
			dest[dest_length++] = escape_char;
			HexFormatUint8Fixed(&dest[dest_length], (uint8_t)src[i]);
			dest_length += 2;
		}
	}

	return dest_length;
}

std::size_t
UriEscape(char *dest, std::span<const std::byte> src,
	  char escape_char) noexcept
{
	return UriEscape(dest, ToStringView(src),
			 escape_char);
}

AllocatedString
UriEscape(std::string_view src, char escape_char) noexcept
{
	/* worst-case allocation - this is a tradeoff; we could count
	   the number of characters to escape first, but that would
	   require iterating the input twice */
	auto buffer = new char[src.size() * 3 + 1];
	size_t length = UriEscape(buffer, src, escape_char);
	buffer[length] = 0;
	return AllocatedString::Donate(buffer);
}

AllocatedString
UriEscape(std::span<const std::byte> src, char escape_char) noexcept
{
	return UriEscape(ToStringView(src), escape_char);
}
