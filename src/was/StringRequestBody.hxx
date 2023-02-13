// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string>

struct was_simple;

namespace Was {

struct RequestBodyTooLarge {};

/**
 * Read the request body into a std::string.
 *
 * Throws #BadRequest if there is no request body, #RequestBodyTooLarge if the
 * given limit is exceeded.
 */
std::string
RequestBodyToString(was_simple *w, std::string::size_type limit);

} // namespace Was
