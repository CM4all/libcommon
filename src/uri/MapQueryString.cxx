/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "MapQueryString.hxx"
#include "Unescape.hxx"
#include "util/AllocatedArray.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringSplit.hxx"

#include <stdexcept>

std::multimap<std::string, std::string>
MapQueryString(std::string_view src)
{
	std::multimap<std::string, std::string> m;

	AllocatedArray<char> unescape_buffer(256);

	for (const std::string_view i : IterableSplitString(src, '&')) {
		const auto [name, escaped_value] = Split(i, '=');
		if (name.empty())
			continue;

		unescape_buffer.GrowDiscard(escaped_value.size());
		char *value = unescape_buffer.data();
		const char *end_value = UriUnescape(value, escaped_value);
		if (end_value == nullptr)
			throw std::runtime_error("Malformed URI escape");

		m.emplace(name, std::string_view{value, std::size_t(end_value - value)});
	}

	return m;
}
