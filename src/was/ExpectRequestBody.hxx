// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

struct was_simple;

namespace Was {

/**
 * Throws #BadRequest if there is no request body or if the
 * Content-Type does not match the specified one.
 */
void
ExpectRequestBody(was_simple *w, const std::string_view content_type);

} // namespace Was
