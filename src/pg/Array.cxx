// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Array.hxx"

#include <stdexcept>

#include <cstring>

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
			const char *end = std::strchr(p, ',');
			if (end == nullptr) {
				end = std::strchr(p, '}');
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
