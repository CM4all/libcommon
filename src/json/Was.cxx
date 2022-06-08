/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
