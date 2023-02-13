// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <string_view>

class NetstringHeader {
	char buffer[32];

public:
	std::string_view operator()(std::size_t size) noexcept;
};
