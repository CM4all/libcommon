// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Read.hxx"
#include "was/ExpectRequestBody.hxx"
#include "was/StringRequestBody.hxx"

#include <nlohmann/json.hpp>

namespace Was {

nlohmann::json
ReadJsonRequestBody(struct was_simple &w, std::size_t limit)
{
	ExpectRequestBody(&w, "application/json");
	const auto s = RequestBodyToString(&w, limit);
	return nlohmann::json::parse(s);
}

} // namespace Was
