// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Write.hxx"
#include "lib/nlohmann_json/Dump.hxx"
#include "was/SimpleResponse.hxx"

extern "C" {
#include <was/simple.h>
}

namespace Was {

bool
WriteJsonResponse(struct was_simple &w,
		  const nlohmann::json &j) noexcept
{
	return was_simple_set_header(&w, "content-type", "application/json") &&
		WriteResponseBody(&w, Json::DumpSloppy(j));
}

} // namespace Was
