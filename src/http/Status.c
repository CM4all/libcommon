/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "Status.h"

const char *const http_status_to_string_data[6][60] = {
	[1] = {
		[HTTP_STATUS_CONTINUE - 100] = "100 Continue",
		[HTTP_STATUS_SWITCHING_PROTOCOLS - 100] = "101 Switching Protocols",
	},
	[2] = {
		[HTTP_STATUS_OK - 200] = "200 OK",
		[HTTP_STATUS_CREATED - 200] = "201 Created",
		[HTTP_STATUS_ACCEPTED - 200] = "202 Accepted",
		[HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION - 200] = "203 Non-Authoritative Information",
		[HTTP_STATUS_NO_CONTENT - 200] = "204 No Content",
		[HTTP_STATUS_RESET_CONTENT - 200] = "205 Reset Content",
		[HTTP_STATUS_PARTIAL_CONTENT - 200] = "206 Partial Content",
		[HTTP_STATUS_MULTI_STATUS - 200] = "207 Multi-Status",
	},
	[3] = {
		[HTTP_STATUS_MULTIPLE_CHOICES - 300] = "300 Multiple Choices",
		[HTTP_STATUS_MOVED_PERMANENTLY - 300] = "301 Moved Permanently",
		[HTTP_STATUS_FOUND - 300] = "302 Found",
		[HTTP_STATUS_SEE_OTHER - 300] = "303 See Other",
		[HTTP_STATUS_NOT_MODIFIED - 300] = "304 Not Modified",
		[HTTP_STATUS_USE_PROXY - 300] = "305 Use Proxy",
		[HTTP_STATUS_TEMPORARY_REDIRECT - 300] = "307 Temporary Redirect",
	},
	[4] = {
		[HTTP_STATUS_BAD_REQUEST - 400] = "400 Bad Request",
		[HTTP_STATUS_UNAUTHORIZED - 400] = "401 Unauthorized",
		[HTTP_STATUS_PAYMENT_REQUIRED - 400] = "402 Payment Required",
		[HTTP_STATUS_FORBIDDEN - 400] = "403 Forbidden",
		[HTTP_STATUS_NOT_FOUND - 400] = "404 Not Found",
		[HTTP_STATUS_METHOD_NOT_ALLOWED - 400] = "405 Method Not Allowed",
		[HTTP_STATUS_NOT_ACCEPTABLE - 400] = "406 Not Acceptable",
		[HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED - 400] = "407 Proxy Authentication Required",
		[HTTP_STATUS_REQUEST_TIMEOUT - 400] = "408 Request Timeout",
		[HTTP_STATUS_CONFLICT - 400] = "409 Conflict",
		[HTTP_STATUS_GONE - 400] = "410 Gone",
		[HTTP_STATUS_LENGTH_REQUIRED - 400] = "411 Length Required",
		[HTTP_STATUS_PRECONDITION_FAILED - 400] = "412 Precondition Failed",
		[HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE - 400] = "413 Request Entity Too Large",
		[HTTP_STATUS_REQUEST_URI_TOO_LONG - 400] = "414 Request-URI Too Long",
		[HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE - 400] = "415 Unsupported Media Type",
		[HTTP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE - 400] = "416 Requested Range Not Satisfiable",
		[HTTP_STATUS_EXPECTATION_FAILED - 400] = "417 Expectation failed",
		[HTTP_STATUS_I_M_A_TEAPOT - 400] = "418 I'm a teapot",
		[HTTP_STATUS_UNPROCESSABLE_ENTITY - 400] = "422 Unprocessable Entity",
		[HTTP_STATUS_LOCKED - 400] = "423 Locked",
		[HTTP_STATUS_FAILED_DEPENDENCY - 400] = "424 Failed Dependency",
		[HTTP_STATUS_UPGRADE_REQUIRED - 400] = "426 Upgrade Required",
		[HTTP_STATUS_PRECONDITION_REQUIRED - 400] = "428 Precondition Required",
		[HTTP_STATUS_TOO_MANY_REQUESTS - 400] = "429 Too Many Requests",
		[HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE - 400] = "431 Request Header Fields Too Large",
		[HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS - 400] = "451 Unavailable for Legal Reasons",
	},
	[5] = {
		[HTTP_STATUS_INTERNAL_SERVER_ERROR - 500] = "500 Internal Server Error",
		[HTTP_STATUS_NOT_IMPLEMENTED - 500] = "501 Not Implemented",
		[HTTP_STATUS_BAD_GATEWAY - 500] = "502 Bad Gateway",
		[HTTP_STATUS_SERVICE_UNAVAILABLE - 500] = "503 Service Unavailable",
		[HTTP_STATUS_GATEWAY_TIMEOUT - 500] = "504 Gateway Timeout",
		[HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED - 500] = "505 HTTP Version Not Supported",
		[HTTP_STATUS_INSUFFICIENT_STORAGE - 500] = "507 Insufficient Storage",
		[HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED - 500] = "511 Network Authentication Required",
	},
};
