// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Was.hxx"
#include "ToDisposableBuffer.hxx"
#include "was/ExceptionResponse.hxx"
#include "was/async/SimpleHandler.hxx"

#include <boost/json.hpp>

using std::string_view_literals::operator""sv;

namespace Was {

[[gnu::pure]]
boost::json::value
ParseJson(const SimpleRequest &request)
{
	if (!request.IsContentType("application/json"sv))
		throw BadRequest{"Wrong request body type\n"sv};

	const std::string_view body = request.body;

	try {
		return boost::json::parse(body);
	} catch (...) {
		throw BadRequest{"JSON parser error\n"sv};
	}
}

void
WriteJson(SimpleResponse &response, const boost::json::value &j) noexcept
{
	response.headers.emplace("content-type"sv, "application/json"sv);
	response.body = Json::ToDisposableBuffer(j);
}

SimpleResponse
ToResponse(const boost::json::value &j) noexcept
{
	SimpleResponse response;
	WriteJson(response, j);
	return response;
}

} // namespace Was
