// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <string>

struct was_simple;

namespace Was {

/**
 * Read the request body into a std::string.
 *
 * Throws #BadRequest if there is no request body, #PayloadTooLarge if
 * the given limit is exceeded.
 */
std::string
RequestBodyToString(was_simple *w, std::string::size_type limit);

} // namespace Was
