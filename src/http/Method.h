/*
 * Copyright 2007-2017 Content Management AG
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

#include <stdbool.h>
#include <assert.h>

typedef enum {
	HTTP_METHOD_NULL = 0,
	HTTP_METHOD_HEAD,
	HTTP_METHOD_GET,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_DELETE,
	HTTP_METHOD_OPTIONS,
	HTTP_METHOD_TRACE,

	/* WebDAV methods */
	HTTP_METHOD_PROPFIND,
	HTTP_METHOD_PROPPATCH,
	HTTP_METHOD_MKCOL,
	HTTP_METHOD_COPY,
	HTTP_METHOD_MOVE,
	HTTP_METHOD_LOCK,
	HTTP_METHOD_UNLOCK,

	/* RFC 5789 */
	HTTP_METHOD_PATCH,

	HTTP_METHOD_INVALID,
} http_method_t;

extern const char *const http_method_to_string_data[HTTP_METHOD_INVALID];

static inline bool
http_method_is_valid(http_method_t method)
{
	return method > HTTP_METHOD_NULL && method < HTTP_METHOD_INVALID;
}

static inline bool
http_method_is_empty(http_method_t method)
{
	/* RFC 2616 4.3: "All responses to the HEAD request method MUST
	   NOT include a message-body, even though the presence of entity
	   header fields might lead one to believe they do." */
	return method == HTTP_METHOD_HEAD;
}

static inline const char *
http_method_to_string(http_method_t method)
{
	assert(method < sizeof(http_method_to_string_data) / sizeof(http_method_to_string_data[0]));
	assert(http_method_to_string_data[method]);

	return http_method_to_string_data[method];
}
