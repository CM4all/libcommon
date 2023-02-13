// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SimpleResponse.hxx"

namespace Was {

bool
WriteResponseBody(struct was_simple *w, std::string_view body) noexcept
{
	return was_simple_set_length(w, body.size()) &&
		was_simple_write(w, body.data(), body.size());
}

bool
SendTextResponse(struct was_simple *w,
		 http_status_t status, std::string_view body) noexcept
{
	return was_simple_status(w, status) &&
		was_simple_set_header(w, "content-type", "text/plain") &&
		WriteResponseBody(w, body);
}

bool
SendNotFound(struct was_simple *w, std::string_view body) noexcept
{
	return SendTextResponse(w, HTTP_STATUS_NOT_FOUND, body);
}

bool
SendBadRequest(struct was_simple *w, std::string_view body) noexcept
{
	return SendTextResponse(w, HTTP_STATUS_BAD_REQUEST, body);
}

bool
SendMethodNotAllowed(struct was_simple *w, const char *allow) noexcept
{
	return was_simple_status(w, HTTP_STATUS_METHOD_NOT_ALLOWED) &&
		was_simple_set_header(w, "allow", allow);
}

} // namespace Was
