// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <map>
#include <string>

/**
 * Parse a query string (or "application/x-www-form-urlencoded") into
 * a multimap.
 *
 * Throws std::invalid_argument if a value has a bad escape.
 */
std::multimap<std::string, std::string, std::less<>>
MapQueryString(std::string_view src);
