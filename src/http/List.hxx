// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

[[gnu::pure]]
bool
http_list_contains(std::string_view list, std::string_view item) noexcept;

/**
 * Case-insensitive version of http_list_contains().
 */
[[gnu::pure]]
bool
http_list_contains_i(std::string_view list, std::string_view item) noexcept;
