// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Read.hxx"
#include "was/ExceptionResponse.hxx"
#include "was/ExpectRequestBody.hxx"
#include "was/Reader.hxx"
#include "was/SimpleResponse.hxx"
#include "json/Parse.hxx"
#include "json/Error.hxx"

#include <boost/json.hpp>

using namespace Was;

boost::json::value
ReadJsonRequestBody(struct was_simple *w)
{
	ExpectRequestBody(w, "application/json");
	WasReader r{w};

	try {
		return Json::Parse(r);
	} catch (const boost::json::system_error &e) {
		if (Json::IsJsonError(e)) {
			SendBadRequest(w, e.what());
			throw AbortResponse{};
		}

		throw;
	}
}
