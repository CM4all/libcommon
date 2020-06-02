/*
 * Copyright 2007-2020 CM4all GmbH
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

#include "Array.hxx"

#include <stdexcept>

#include <string.h>

namespace Pg {

std::forward_list<std::string>
DecodeArray(const char *p)
{
	std::forward_list<std::string> dest;
	auto i = dest.before_begin();

	if (p == nullptr || *p == 0)
		return dest;

	if (*p != '{')
		throw std::invalid_argument("'{' expected");

	if (p[1] == '}' && p[2] == 0)
		return dest; /* special case: empty array */

	do {
		++p;

		if (*p == '\"') {
			++p;

			std::string value;

			while (*p != '\"') {
				if (*p == '\\') {
					++p;

					if (*p == 0)
						throw std::invalid_argument("backslash at end of string");

					value.push_back(*p++);
				} else if (*p == 0) {
					throw std::invalid_argument("missing closing double quote");
				} else {
					value.push_back(*p++);
				}
			}

			++p;

			if (*p != '}' && *p != ',')
				throw std::invalid_argument("'}' or ',' expected");

			i = dest.emplace_after(i, std::move(value));
		} else if (*p == 0) {
			throw std::invalid_argument("missing '}'");
		} else if (*p == '{') {
			throw std::invalid_argument("unexpected '{'");
		} else {
			const char *end = strchr(p, ',');
			if (end == nullptr) {
				end = strchr(p, '}');
				if (end == nullptr)
					throw std::invalid_argument("missing '}'");
			}

			i = dest.emplace_after(i, std::string(p, end));

			p = end;
		}
	} while (*p == ',');

	if (*p != '}')
		throw std::invalid_argument("'}' expected");

	++p;

	if (*p != 0)
		throw std::invalid_argument("garbage after '}'");

	return dest;
}

} /* namespace Pg */
