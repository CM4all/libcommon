// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "QuoteIdentifier.hxx"

namespace Pg {

std::string
QuoteIdentifier(std::string_view s) noexcept
{
	std::string result;
	result.reserve(s.size() + 2);
	result.push_back('"');
	for (const char ch : s) {
		if (ch == '"')
			result.push_back('"');
		result.push_back(ch);
	}
	result.push_back('"');
	return result;
}

} /* namespace Pg */
