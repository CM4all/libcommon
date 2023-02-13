// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Status.hxx"

static constexpr struct {
	HttpStatus status;
	const char *text;
} http_status_to_string_input[] = {
	{ HttpStatus::CONTINUE, "100 Continue" },
	{ HttpStatus::SWITCHING_PROTOCOLS, "101 Switching Protocols" },
	{ HttpStatus::OK, "200 OK" },
	{ HttpStatus::CREATED, "201 Created" },
	{ HttpStatus::ACCEPTED, "202 Accepted" },
	{ HttpStatus::NON_AUTHORITATIVE_INFORMATION,
	  "203 Non-Authoritative Information" },
	{ HttpStatus::NO_CONTENT, "204 No Content" },
	{ HttpStatus::RESET_CONTENT, "205 Reset Content" },
	{ HttpStatus::PARTIAL_CONTENT, "206 Partial Content" },
	{ HttpStatus::MULTI_STATUS, "207 Multi-Status" },
	{ HttpStatus::MULTIPLE_CHOICES, "300 Multiple Choices" },
	{ HttpStatus::MOVED_PERMANENTLY, "301 Moved Permanently" },
	{ HttpStatus::FOUND, "302 Found" },
	{ HttpStatus::SEE_OTHER, "303 See Other" },
	{ HttpStatus::NOT_MODIFIED, "304 Not Modified" },
	{ HttpStatus::USE_PROXY, "305 Use Proxy" },
	{ HttpStatus::TEMPORARY_REDIRECT, "307 Temporary Redirect" },
	{ HttpStatus::BAD_REQUEST, "400 Bad Request" },
	{ HttpStatus::UNAUTHORIZED, "401 Unauthorized" },
	{ HttpStatus::PAYMENT_REQUIRED, "402 Payment Required" },
	{ HttpStatus::FORBIDDEN, "403 Forbidden" },
	{ HttpStatus::NOT_FOUND, "404 Not Found" },
	{ HttpStatus::METHOD_NOT_ALLOWED, "405 Method Not Allowed" },
	{ HttpStatus::NOT_ACCEPTABLE, "406 Not Acceptable" },
	{ HttpStatus::PROXY_AUTHENTICATION_REQUIRED,
	  "407 Proxy Authentication Required" },
	{ HttpStatus::REQUEST_TIMEOUT, "408 Request Timeout" },
	{ HttpStatus::CONFLICT, "409 Conflict" },
	{ HttpStatus::GONE, "410 Gone" },
	{ HttpStatus::LENGTH_REQUIRED, "411 Length Required" },
	{ HttpStatus::PRECONDITION_FAILED, "412 Precondition Failed" },
	{ HttpStatus::REQUEST_ENTITY_TOO_LARGE,
	  "413 Request Entity Too Large" },
	{ HttpStatus::REQUEST_URI_TOO_LONG, "414 Request-URI Too Long" },
	{ HttpStatus::UNSUPPORTED_MEDIA_TYPE, "415 Unsupported Media Type" },
	{ HttpStatus::REQUESTED_RANGE_NOT_SATISFIABLE,
	  "416 Requested Range Not Satisfiable" },
	{ HttpStatus::EXPECTATION_FAILED, "417 Expectation failed" },
	{ HttpStatus::I_M_A_TEAPOT, "418 I'm a teapot" },
	{ HttpStatus::UNPROCESSABLE_ENTITY, "422 Unprocessable Entity" },
	{ HttpStatus::LOCKED, "423 Locked" },
	{ HttpStatus::FAILED_DEPENDENCY, "424 Failed Dependency" },
	{ HttpStatus::UPGRADE_REQUIRED, "426 Upgrade Required" },
	{ HttpStatus::PRECONDITION_REQUIRED, "428 Precondition Required" },
	{ HttpStatus::TOO_MANY_REQUESTS, "429 Too Many Requests" },
	{ HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE,
	  "431 Request Header Fields Too Large" },
	{ HttpStatus::UNAVAILABLE_FOR_LEGAL_REASONS,
	  "451 Unavailable for Legal Reasons" },
	{ HttpStatus::INTERNAL_SERVER_ERROR, "500 Internal Server Error" },
	{ HttpStatus::NOT_IMPLEMENTED, "501 Not Implemented" },
	{ HttpStatus::BAD_GATEWAY, "502 Bad Gateway" },
	{ HttpStatus::SERVICE_UNAVAILABLE, "503 Service Unavailable" },
	{ HttpStatus::GATEWAY_TIMEOUT, "504 Gateway Timeout" },
	{ HttpStatus::HTTP_VERSION_NOT_SUPPORTED,
	  "505 HTTP Version Not Supported" },
	{ HttpStatus::INSUFFICIENT_STORAGE, "507 Insufficient Storage" },
	{ HttpStatus::NETWORK_AUTHENTICATION_REQUIRED,
	  "511 Network Authentication Required" },
};

static constexpr auto
MakeHttpStatusToStringData() noexcept
{
	std::array<std::array<const char *, 60>, 6> result{};

	for (const auto &i : http_status_to_string_input) {
		const auto status = static_cast<unsigned>(i.status);
		result[status / 100][status % 100] = i.text;
	}

	return result;
}

constinit const std::array<std::array<const char *, 60>, 6> http_status_to_string_data =
	MakeHttpStatusToStringData();
