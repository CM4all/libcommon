// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "FormRequestBody.hxx"
#include "ExceptionResponse.hxx"
#include "ExpectRequestBody.hxx"
#include "StringRequestBody.hxx"
#include "uri/MapQueryString.hxx"

#include <stdexcept>

namespace Was {

std::multimap<std::string, std::string, std::less<>>
FormRequestBodyToMap(was_simple *w, std::string::size_type limit)
{
	ExpectRequestBody(w, "application/x-www-form-urlencoded");

	const auto raw = RequestBodyToString(w, limit);

	try {
		return MapQueryString(raw);
	} catch (const std::invalid_argument &e) {
		throw BadRequest{"Malformed request body\n"};
	}
}

} // namespace Was
