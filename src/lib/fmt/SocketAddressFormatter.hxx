// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/SocketAddress.hxx"
#include "net/ToString.hxx"

#include <fmt/format.h>

#include <concepts>

template<typename T>
requires std::convertible_to<T, SocketAddress>
struct fmt::formatter<T> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(SocketAddress address, FormatContext &ctx) {
		char buffer[256];
		std::string_view s;

		if (ToString(buffer, sizeof(buffer), address))
			s = buffer;
		else
			s = "?";

		return formatter<string_view>::format(s, ctx);
	}
};
