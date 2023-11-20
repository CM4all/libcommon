// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Write.hxx"
#include "was/SimpleResponse.hxx"

extern "C" {
#include <was/simple.h>
}

#include <nlohmann/json.hpp>

namespace Was {

bool
WriteJsonResponse(struct was_simple &w,
		  const nlohmann::json &j) noexcept
{
	return was_simple_set_header(&w, "content-type", "application/json") &&
		WriteResponseBody(&w, j.dump());
}

} // namespace Was
