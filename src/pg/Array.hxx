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

/*
 * Small utilities for PostgreSQL clients.
 */

#pragma once

#include <forward_list>
#include <string>

namespace Pg {

/**
 * Throws std::invalid_argument on syntax error.
 */
std::forward_list<std::string>
DecodeArray(const char *p);

template<typename L>
requires std::convertible_to<typename L::value_type, std::string_view> && std::forward_iterator<typename L::const_iterator>
std::string
EncodeArray(const L &src) noexcept
{
	if (src.empty())
		return "{}";

	std::string dest("{");

	bool first = true;
	for (const auto &i : src) {
		if (first)
			first = false;
		else
			dest.push_back(',');

		dest.push_back('"');

		for (const auto ch : i) {
			if (ch == '\\' || ch == '"')
				dest.push_back('\\');
			dest.push_back(ch);
		}

		dest.push_back('"');
	}

	dest.push_back('}');
	return dest;
}

} /* namespace Pg */
