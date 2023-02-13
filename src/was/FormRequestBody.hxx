// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <map>
#include <string>

struct was_simple;

namespace Was {

/**
 * Parse an "application/x-www-form-urlencoded" request body into a
 * std::multimap.
 *
 * Throws #BadRequest if there is no request body or if the
 * Content-Type is not "application/x-www-form-urlencoded",
 * #RequestBodyTooLarge if the given limit is exceeded.
 */
std::multimap<std::string, std::string, std::less<>>
FormRequestBodyToMap(was_simple *w, std::string::size_type limit);

} // namespace Was
