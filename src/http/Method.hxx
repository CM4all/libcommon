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

#pragma once

#include <cassert>
#include <cstdint>

enum class HttpMethod : uint_least8_t {
	/* The values below are part of the logging protocol (see
	   net/log/Protocol.hxx); it must be kept stable and in this
	   order.  Add new values at the end, right before
	   #INVALID. */

	/**
	 * Not an actual HTTP method, but a "magic" value which means
	 * a variable explicitly has no value.  This can be used as an
	 * initializer if you later need to check whether the variable
	 * has been set to a meaningful value.
	 */
	UNDEFINED = 0,

	HEAD,
	GET,
	POST,
	PUT,
	DELETE,
	OPTIONS,
	TRACE,

	/* WebDAV methods */
	PROPFIND,
	PROPPATCH,
	MKCOL,
	COPY,
	MOVE,
	LOCK,
	UNLOCK,

	/* RFC 5789 */
	PATCH,

	/* Versioning Extensions to WebDAV methods (RFC3253) */
	REPORT,

	INVALID,
};

constexpr const char *http_method_to_string_data[] = {
	nullptr,

	"HEAD",
	"GET",
	"POST",
	"PUT",
	"DELETE",
	"OPTIONS",
	"TRACE",

	/* WebDAV methods */
	"PROPFIND",
	"PROPPATCH",
	"MKCOL",
	"COPY",
	"MOVE",
	"LOCK",
	"UNLOCK",

	/* RFC 5789 */
	"PATCH",

	/* Versioning Extensions to WebDAV methods (RFC3253) */
	"REPORT",
};

static_assert(sizeof(http_method_to_string_data) / sizeof(http_method_to_string_data[0]) == static_cast<unsigned>(HttpMethod::INVALID));

static constexpr bool
http_method_is_valid(HttpMethod method) noexcept
{
	return method > HttpMethod::UNDEFINED && method < HttpMethod::INVALID;
}

static constexpr bool
http_method_is_empty(HttpMethod method) noexcept
{
	/* RFC 2616 4.3: "All responses to the HEAD request method MUST
	   NOT include a message-body, even though the presence of entity
	   header fields might lead one to believe they do." */
	return method == HttpMethod::HEAD;
}

static constexpr const char *
http_method_to_string(HttpMethod _method) noexcept
{
	const unsigned method = static_cast<unsigned>(_method);
	assert(method < sizeof(http_method_to_string_data) / sizeof(http_method_to_string_data[0]));
	assert(http_method_to_string_data[method]);

	return http_method_to_string_data[method];
}
