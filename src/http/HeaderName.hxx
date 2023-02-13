// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

/**
 * Determines if the specified name consists only of valid characters
 * (RFC 822 3.2).
 */
[[gnu::pure]]
bool
http_header_name_valid(const char *name) noexcept;

[[gnu::pure]]
bool
http_header_name_valid(std::string_view name) noexcept;

/**
 * Determines if the specified name is a hop-by-hop header.  In
 * addition to the list in RFC 2616 13.5.1, "Content-Length" is also a
 * hop-by-hop header according to this function.
 */
[[gnu::pure]]
bool
http_header_is_hop_by_hop(const char *name) noexcept;
