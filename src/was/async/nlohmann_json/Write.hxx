// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <nlohmann/json_fwd.hpp>

namespace Was {

struct SimpleResponse;

/**
 * Serialize the given JSON value and place it as the response body
 * and add header "Content-Type: application/json".
 */
void
WriteJson(SimpleResponse &response, const nlohmann::json &j) noexcept;

[[gnu::pure]]
SimpleResponse
ToResponse(const nlohmann::json &j) noexcept;

} // namespace Was
