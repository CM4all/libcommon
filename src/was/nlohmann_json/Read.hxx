// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstddef> // for std::size_t

struct was_simple;

namespace Was {

/**
 * Read the request body into a nlohmann::json.
 *
 * Throws #BadRequest if there is no request body,
 * #RequestBodyTooLarge if the given limit is exceeded.
 */
nlohmann::json
ReadJsonRequestBody(struct was_simple &w,
		    std::size_t limit=1024*1024);

} // namespace Was
