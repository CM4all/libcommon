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

#include "Escape.hxx"
#include "uri/Chars.hxx"
#include "util/AllocatedString.hxx"
#include "util/StringView.hxx"
#include "util/HexFormat.hxx"

std::size_t
UriEscape(char *dest, StringView src,
	  char escape_char) noexcept
{
	std::size_t dest_length = 0;

	for (std::size_t i = 0; i < src.size; ++i) {
		if (IsUriUnreservedChar(src[i])) {
			dest[dest_length++] = src[i];
		} else {
			dest[dest_length++] = escape_char;
			format_uint8_hex_fixed(&dest[dest_length], (uint8_t)src[i]);
			dest_length += 2;
		}
	}

	return dest_length;
}

std::size_t
UriEscape(char *dest, ConstBuffer<void> src,
	  char escape_char) noexcept
{
	return UriEscape(dest, StringView((const char *)src.data, src.size),
			 escape_char);
}

AllocatedString
UriEscape(StringView src, char escape_char) noexcept
{
	/* worst-case allocation - this is a tradeoff; we could count
	   the number of characters to escape first, but that would
	   require iterating the input twice */
	auto buffer = new char[src.size * 3 + 1];
	size_t length = UriEscape(buffer, src, escape_char);
	buffer[length] = 0;
	return AllocatedString::Donate(buffer);
}

AllocatedString
UriEscape(ConstBuffer<void> src, char escape_char) noexcept
{
	return UriEscape(StringView{ConstBuffer<char>::FromVoid(src)},
			 escape_char);
}
