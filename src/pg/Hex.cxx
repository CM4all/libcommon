// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

/*
 * Small utilities for PostgreSQL clients.
 */

#include "Hex.hxx"
#include "util/AllocatedArray.hxx"
#include "util/HexParse.hxx"
#include "util/StringCompare.hxx"

#include <stdexcept>

using std::string_view_literals::operator""sv;

template<typename T> class AllocatedArray;

namespace Pg {

AllocatedArray<std::byte>
DecodeHex(std::string_view src)
{
        if (!SkipPrefix(src, "\\x"sv))
		throw std::invalid_argument{"Missing hex prefix"};

	if (src.size() % 2 != 0)
		throw std::invalid_argument{"Odd length"};

	AllocatedArray<std::byte> result{src.size() / 2};

	const char *p = src.data();
	for (auto &i : result) {
		p = ParseLowerHexFixed(p, i);
		if (p == nullptr)
			throw std::invalid_argument{"Malformed hex digit"};
	}

	return result;
}

} /* namespace Pg */
