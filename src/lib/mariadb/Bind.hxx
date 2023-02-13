// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <mysql.h>

#include <array>
#include <string_view>

template <std::size_t size>
class MysqlStringBuffer {
	std::array<char, size> value;

	unsigned long length;

public:
	constexpr operator MYSQL_BIND() noexcept {
		MYSQL_BIND bind{};
		bind.buffer_type = MYSQL_TYPE_STRING;
		bind.buffer = value.data();
		bind.buffer_length = value.size();
		bind.length = &length;
		return bind;
	}

	constexpr operator std::string_view() const noexcept {
		return {value.data(), length};
	}
};
