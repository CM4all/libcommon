// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <nlohmann/json_fwd.hpp>

namespace Was {

struct SimpleRequest;

/**
 * Parse a JSON request body.  Throws #BadRequest if the request body
 * is not JSON (according to the Content-Type header) and on JSON
 * parser errors.
 */
nlohmann::json
ParseJson(const SimpleRequest &request);

} // namespace Was
