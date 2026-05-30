// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <was/simple.h>

#include <cstdint>
#include <initializer_list>
#include <string_view>

struct was_simple;

namespace Was {

static inline bool
Write(struct was_simple &w, std::string_view src) noexcept
{
	return was_simple_write(&w, src.data(), src.size());
}

static inline bool
Write(struct was_simple &w, std::initializer_list<std::string_view> list) noexcept
{
	uint_least64_t total_length = 0;
	for (const std::string_view i : list)
		total_length += i.size();

	if (!was_simple_set_length(&w, total_length)) [[unlikely]]
		return false;

	for (const std::string_view i : list)
		if (!Write(w, i)) [[unlikely]]
			return false;

	return true;
}

} // namespace Was
