// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Verify URI parts.
 */

#pragma once

#include <string_view>

/**
 * Is this a valid domain label (i.e. host name segment) according to
 * RFC 1034 3.5?
 */
[[gnu::pure]]
bool
VerifyDomainLabel(std::string_view s) noexcept;

/**
 * Like VerifyDomainLabel(), but don't allow upper case letters.
 */
[[gnu::pure]]
bool
VerifyLowerDomainLabel(std::string_view s) noexcept;

/**
 * Is this a valid domain name (i.e. host name) according to RFC 1034
 * 3.5?
 */
[[gnu::pure]]
bool
VerifyDomainName(std::string_view name) noexcept;

/**
 * Like VerifyDomainName(), but don't allow upper case letters.
 */
[[gnu::pure]]
bool
VerifyLowerDomainName(std::string_view name) noexcept;

/**
 * Is this a valid "host:port" string (or "Host:" request header)
 * according to RFC 2616 3.2.2 / 14.23?
 */
[[gnu::pure]]
bool
VerifyUriHostPort(std::string_view host_port) noexcept;

/**
 * Verifies one path segment of an URI according to RFC 3986,
 * "segment".
 */
[[gnu::pure]]
bool
uri_segment_verify(std::string_view segment) noexcept;

/**
 * Verifies the path portion of an URI according to RFC 3986 3.3,
 * "path-absolute".
 */
[[gnu::pure]]
bool
uri_path_verify(std::string_view uri) noexcept;

/**
 * Performs some paranoid checks on the URI; the following is not
 * allowed:
 *
 * - %00
 * - %2f (encoded slash)
 * - "/../", "/./"
 * - "/..", "/." at the end
 *
 * It is assumed that the URI was already verified with
 * uri_path_verify().
 */
[[gnu::pure]]
bool
uri_path_verify_paranoid(const char *uri) noexcept;

/**
 * Quickly verify the validity of an URI (path plus query).  This may
 * be used before passing it to another server, not to be parsed by
 * this process.
 */
[[gnu::pure]]
bool
uri_path_verify_quick(const char *uri) noexcept;

/**
 * Verify whether the given string is a valid query according to RFC
 * 3986 3.4, "query".
 */
[[gnu::pure]]
bool
VerifyUriQuery(std::string_view query) noexcept;

/**
 * Verify whether the given string is a (syntactically) valid absolute
 * "http://" or "https://" URL.  It does not allow a fragment
 * identifier.
 */
[[gnu::pure]]
bool
VerifyHttpUrl(std::string_view url) noexcept;
