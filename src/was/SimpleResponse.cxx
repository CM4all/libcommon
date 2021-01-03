/*
 * Copyright 2019-2021 CM4all GmbH
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
