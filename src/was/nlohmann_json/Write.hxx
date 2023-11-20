// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <nlohmann/json_fwd.hpp>

struct was_simple;

namespace Was {

bool
WriteJsonResponse(struct was_simple &w,
		  const nlohmann::json &j) noexcept;

} // namespace Was
