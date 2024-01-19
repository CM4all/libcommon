// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Read.hxx"
#include "was/ExceptionResponse.hxx"
#include "was/async/SimpleHandler.hxx"

#include <nlohmann/json.hpp>

using std::string_view_literals::operator""sv;

namespace Was {

[[gnu::pure]]
nlohmann::json
ParseJson(const SimpleRequest &request)
{
	if (!request.IsContentType("application/json"sv))
		throw BadRequest{"Wrong request body type\n"sv};

	const std::string_view body = request.body;

	try {
		return nlohmann::json::parse(body);
	} catch (...) {
		throw BadRequest{"JSON parser error\n"sv};
	}
}

} // namespace Was
