// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "NetstringGenerator.hxx"

#include <cstddef>
#include <string_view>

using std::string_view_literals::operator""sv;

template<typename L>
static inline size_t
GetTotalSize(const L &list) noexcept
{
	size_t result = 0;
	for (const auto &i : list)
		result += i.size();
	return result;
}

void
NetstringGenerator::operator()(std::list<std::span<const std::byte>> &list,
			       bool comma) noexcept
{
	list.emplace_front(std::as_bytes(std::span{header(GetTotalSize(list))}));

	if (comma)
		list.emplace_back(std::as_bytes(std::span{","sv}));
}
