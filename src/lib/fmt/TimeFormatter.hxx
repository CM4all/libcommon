// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "time/ISO8601.hxx"
#include "util/StringBuffer.hxx"

#include <fmt/format.h>

template<>
struct fmt::formatter<std::chrono::system_clock::time_point> : formatter<const char *>
{
	template<typename FormatContext>
	auto format(std::chrono::system_clock::time_point tp, FormatContext &ctx) const {
		return formatter<const char *>::format(FormatISO8601(tp).c_str(), ctx);
	}
};
