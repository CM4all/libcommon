// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <string_view>

enum class HttpStatus : uint_least16_t {
	/**
	 * Not an actual HTTP status code, but a "magic" value which
	 * means this status has no value.  This can be used as an
	 * initializer.
	 */
	UNDEFINED = 0,

	CONTINUE = 100,
	SWITCHING_PROTOCOLS = 101,

	// added in RFC 2518, removed in RFC 4918 (WebDAV)
	PROCESSING = 102,

	// added in RFC 2518, removed in RFC 4918 (WebDAV)
	EARLY_HINTS = 103,

	OK = 200,
	CREATED = 201,
	ACCEPTED = 202,
	NON_AUTHORITATIVE_INFORMATION = 203,
	NO_CONTENT = 204,
	RESET_CONTENT = 205,
	PARTIAL_CONTENT = 206,

	/**
	 * @see RFC 4918 (WebDAV)
	 */
	MULTI_STATUS = 207,

	/**
	 * @see RFC 5842 (Binding Extensiosn to WebDAV)
	 */
	ALREADY_REPORTED = 208,

	/**
	 * @see RFC 3229 (Delta Encoding in HTTP)
	 */
	IM_USED = 226,

	MULTIPLE_CHOICES = 300,
	MOVED_PERMANENTLY = 301,
	FOUND = 302,
	SEE_OTHER = 303,
	NOT_MODIFIED = 304,
	USE_PROXY = 305, // deprecated in RFC 7231
	TEMPORARY_REDIRECT = 307,
	PERMANENT_REDIRECT = 308, // RFC 9110 (HTTP Semantics)
	BAD_REQUEST = 400,
	UNAUTHORIZED = 401,
	PAYMENT_REQUIRED = 402,
	FORBIDDEN = 403,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	NOT_ACCEPTABLE = 406,
	PROXY_AUTHENTICATION_REQUIRED = 407,
	REQUEST_TIMEOUT = 408,
	CONFLICT = 409,
	GONE = 410,
	LENGTH_REQUIRED = 411,
	PRECONDITION_FAILED = 412,
	PAYLOAD_TOO_LARGE = 413,
	REQUEST_URI_TOO_LONG = 414,
	UNSUPPORTED_MEDIA_TYPE = 415,
	REQUESTED_RANGE_NOT_SATISFIABLE = 416,
	EXPECTATION_FAILED = 417,

	/**
	 * @see RFC 2324
	 */
	I_M_A_TEAPOT = 418,

	/**
	 * @see RFC 9110 (HTTP Semantics)
	 */
	MISDIRECTED_REQUEST = 421,

	/**
	 * @see RFC 4918 (WebDAV)
	 */
	UNPROCESSABLE_ENTITY = 422,

	/**
	 * @see RFC 4918 (WebDAV)
	 */
	LOCKED = 423,

	/**
	 * @see RFC 4918 (WebDAV)
	 */
	FAILED_DEPENDENCY = 424,

	/**
	 * @see RFC 8470 (Using Early Data in HTTP)
	 */
	TOO_EARLY = 425,

	/**
	 * @see RFC 7231 (HTTP 1.1)
	 */
	UPGRADE_REQUIRED = 426,

	/**
	 * @see RFC 6585 (Additional HTTP Status Codes)
	 */
	PRECONDITION_REQUIRED = 428,

	/**
	 * @see RFC 6585 (Additional HTTP Status Codes)
	 */
	TOO_MANY_REQUESTS = 429,

	/**
	 * @see RFC 6585 (Additional HTTP Status Codes)
	 */
	REQUEST_HEADER_FIELDS_TOO_LARGE = 431,

	/**
	 * @see https://datatracker.ietf.org/doc/draft-ietf-httpbis-legally-restricted-status/
	 */
	UNAVAILABLE_FOR_LEGAL_REASONS = 451,

	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	BAD_GATEWAY = 502,
	SERVICE_UNAVAILABLE = 503,
	GATEWAY_TIMEOUT = 504,
	HTTP_VERSION_NOT_SUPPORTED = 505,

	/**
	 * @see RFC 2295 (Transparent Content Negotiation in HTTP)
	 */
	VARIANT_ALSO_NEGOTIATES = 506,

	/**
	 * @see RFC 4918 (WebDAV)
	 */
	INSUFFICIENT_STORAGE = 507,

	/**
	 * @see RFC 5842 (Binding Extensions to WebDAV)
	 */
	LOOP_DETECTED = 508,

	/**
	 * @see RFC 2774 (An HTTP Extension Framework)
	 */
	NOT_EXTENDED = 510,

	/**
	 * @see RFC 6585 (Additional HTTP Status Codes)
	 */
	NETWORK_AUTHENTICATION_REQUIRED = 511,
};

extern const std::array<std::array<std::string_view, 60>, 6> http_status_to_string_data;

static inline bool
http_status_is_valid(HttpStatus _status) noexcept
{
	const auto status = static_cast<unsigned>(_status);

	return (status / 100) < http_status_to_string_data.size() &&
		status % 100 < http_status_to_string_data.front().size() &&
	       !http_status_to_string_data[status / 100][status % 100].empty();
}

[[gnu::const]]
static inline std::string_view
http_status_to_string(HttpStatus _status) noexcept
{
	assert(http_status_is_valid(_status));

	const auto status = static_cast<unsigned>(_status);
	return http_status_to_string_data[status / 100][status % 100];
}

static constexpr bool
http_status_is_success(HttpStatus _status) noexcept
{
	const auto status = static_cast<unsigned>(_status);
	return status >= 200 && status < 300;
}

static constexpr bool
http_status_is_redirect(HttpStatus _status) noexcept
{
	const auto status = static_cast<unsigned>(_status);
	return status >= 300 && status < 400;
}

static constexpr bool
http_status_is_client_error(HttpStatus _status) noexcept
{
	const auto status = static_cast<unsigned>(_status);
	return status >= 400 && status < 500;
}

static constexpr bool
http_status_is_server_error(HttpStatus _status) noexcept
{
	const auto status = static_cast<unsigned>(_status);
	return status >= 500 && status < 600;
}

static constexpr bool
http_status_is_error(HttpStatus _status) noexcept
{
	const auto status = static_cast<unsigned>(_status);
	return status >= 400 && status < 600;
}

static constexpr bool
http_status_is_empty(HttpStatus status) noexcept
{
	return status == HttpStatus::CONTINUE ||
		status == HttpStatus::NO_CONTENT ||
		status == HttpStatus::RESET_CONTENT ||
		status == HttpStatus::NOT_MODIFIED;
}
