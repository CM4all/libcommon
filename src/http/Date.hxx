// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Format and parse HTTP dates.
 */

#pragma once

#include <chrono>

void
http_date_format_r(char *buffer,
		   std::chrono::system_clock::time_point t) noexcept;

[[gnu::const]]
const char *
http_date_format(std::chrono::system_clock::time_point t) noexcept;

[[gnu::pure]]
std::chrono::system_clock::time_point
http_date_parse(const char *p) noexcept;
