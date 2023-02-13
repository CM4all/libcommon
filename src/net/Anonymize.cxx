// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
