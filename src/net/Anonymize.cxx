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

#include "Anonymize.hxx"
#include "util/CharUtil.hxx"
#include "util/StringSplit.hxx"

#include <iterator> // for std::distance()

using std::string_view_literals::operator""sv;

std::pair<std::string_view, std::string_view>
AnonymizeAddress(std::string_view value) noexcept
{
	if (const auto [_, rest] = Split(value, '.');
	    rest.data() != nullptr && IsDigitASCII(value.front()) &&
	    IsDigitASCII(value.back())) {
		/* IPv4: zero the last octet */

		const auto q = rest.rfind('.');
		if (q != rest.npos)
			return {
				value.substr(0, std::distance(value.data(),
							      rest.data() + q + 1)),
				"0"sv,
			};
	}

	if (auto [_, rest] = Split(value, ':');
	    rest.data() != nullptr &&
	    (IsHexDigit(value.front()) || value.front() == ':') &&
	    (IsHexDigit(value.back()) || value.back() == ':')) {
		/* IPv6: truncate after the first 40 bit */

		for (unsigned i = 1; i < 2; ++i) {
			const auto q = rest.find(':');
			if (q == 0)
				return {
					value.substr(0, std::distance(value.data(),
								      rest.data() + q + 1)),
					{},
				};

			if (q == rest.npos)
				return {value, {}};

			rest = rest.substr(q + 1);
		}

		/* clear the low 8 bit of the third segment */
		const auto third_segment = Split(rest, ':').first;
		if (third_segment.size() > 2)
			return {
				value.substr(0, std::distance(value.begin(),
							      std::prev(third_segment.end(), 2))),
				"00::"sv,
			};

		return {
			value.substr(0, std::distance(value.data(), rest.data())),
			":"sv,
		};
	}

	return {value, {}};
}
