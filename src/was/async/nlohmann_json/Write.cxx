// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Write.hxx"
#include "lib/nlohmann_json/Dump.hxx"
#include "was/ExceptionResponse.hxx"
#include "was/async/SimpleResponse.hxx"
#include "was/async/StringOutputProducer.hxx"

using std::string_view_literals::operator""sv;

namespace Was {

void
WriteJson(SimpleResponse &response, const nlohmann::json &j) noexcept
{
	response.headers.emplace("content-type"sv, "application/json"sv);
	response.body = std::make_unique<StringOutputProducer>(Json::DumpSloppy(j));
}

SimpleResponse
ToResponse(const nlohmann::json &j) noexcept
{
	SimpleResponse response;
	WriteJson(response, j);
	return response;
}

} // namespace Was
