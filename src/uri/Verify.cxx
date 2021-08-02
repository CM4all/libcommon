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

#include "Verify.hxx"
#include "Chars.hxx"
#include "util/CharUtil.hxx"
#include "util/Compiler.h"
#include "util/StringVerify.hxx"
#include "util/StringView.hxx"

#include <algorithm>

static constexpr bool
IsAlphaNumericDashASCII(char ch) noexcept
{
    return IsAlphaNumericASCII(ch) || ch == '-';
}

/**
 * Is this a valid domain label (i.e. host name segment) according to
 * RFC 1034 3.5?
 */
#if !GCC_OLDER_THAN(10,0)
constexpr
#endif
static bool
VerifyDomainLabel(StringView s) noexcept
{
	/* RFC 1035 2.3.4: domain labels are limited to 63 octets */
	if (s.empty() || s.size > 63)
		return false;

	if (!IsAlphaNumericASCII(s.front()))
		return false;

	s.pop_front();
	if (s.empty())
		return true;

	if (!IsAlphaNumericASCII(s.back()))
		return false;

	s.pop_back();
	return std::all_of(s.begin(), s.end(), IsAlphaNumericDashASCII);
}

bool
VerifyDomainName(std::string_view s) noexcept
{
	/* RFC 1035 2.3.4: domain names are limited to 255 octets */

	return s.size() <= 255 && IsNonEmptyListOf(s, '.', VerifyDomainLabel);
}

[[gnu::pure]]
static bool
VerifyPort(StringView s) noexcept
{
	return !s.empty() && s.size <= 5 &&
		std::all_of(s.begin(), s.end(), IsDigitASCII);
}

[[gnu::pure]]
static bool
VerifyIPv6Segment(StringView s) noexcept
{
	return s.size <= 4 && std::all_of(s.begin(), s.end(), IsHexDigit);
}

[[gnu::pure]]
static bool
VerifyIPv6(StringView host) noexcept
{
	return host.size < 40 &&
		IsNonEmptyListOf(host, ':', VerifyIPv6Segment);
}

[[gnu::pure]]
static bool
VerifyUriHost(StringView host) noexcept
{
	if (host.Find(':'))
		return VerifyIPv6(host);

	return VerifyDomainName(host);
}

bool
VerifyUriHostPort(std::string_view _host_port) noexcept
{
	const StringView host_port{_host_port};

	if (host_port.empty())
		return false;

	if (host_port.front() == '[') {
		auto [host, port] = host_port.substr(1).Split(']');
		if (port == nullptr)
			/* syntax error: the closing bracket was not
			   found */
			return false;

		if (!port.empty()) {
			if (port.front() != ':')
				return false;

			port.pop_front();
			if (!VerifyPort(port))
				return false;
		}

		return VerifyUriHost(host);
	} else {
		auto [host, port] = host_port.SplitLast(':');
		if (host.Find(':') != nullptr)
			/* more than one colon: assume this is a
			   numeric IPv6 address (without a port
			   specification) */
			return VerifyIPv6(host_port);

		return VerifyUriHost(host) &&
			(port == nullptr || VerifyPort(port));
	}
}

bool
uri_segment_verify(std::string_view segment) noexcept
{
	for (char ch : segment) {
		/* XXX check for invalid escaped characters? */

		if (!IsUriPchar(ch))
			return false;
	}

	return true;
}

bool
uri_path_verify(std::string_view _uri) noexcept
{
	StringView uri(_uri);

	if (uri.empty() || uri.front() != '/')
		/* path must begin with slash */
		return false;

	uri.pop_front(); // strip the leading slash

	do {
		auto s = uri.Split('/');
		if (!uri_segment_verify(s.first))
			return false;

		uri = s.second;
	} while (!uri.empty());

	return true;
}

static constexpr bool
IsEncodedNul(const char *p) noexcept
{
	return p[0] == '%' && p[1] == '0' && p[2] == '0';
}

static constexpr bool
IsEncodedDot(const char *p) noexcept
{
	return p[0] == '%' && p[1] == '2' &&
		(p[2] == 'e' || p[2] == 'E');
}

static constexpr bool
IsEncodedSlash(const char *p) noexcept
{
	return p[0] == '%' && p[1] == '2' &&
		(p[2] == 'f' || p[2] == 'F');
}

bool
uri_path_verify_paranoid(const char *uri) noexcept
{
	if (uri[0] == '.' &&
	    (uri[1] == 0 || uri[1] == '/' ||
	     (uri[1] == '.' && (uri[2] == 0 || uri[2] == '/')) ||
	     IsEncodedDot(uri + 1)))
		/* no ".", "..", "./", "../" */
		return false;

	if (IsEncodedDot(uri))
		return false;

	while (*uri != 0) {
		if (*uri == '%') {
			if (/* don't allow an encoded NUL character */
			    IsEncodedNul(uri) ||
			    /* don't allow an encoded slash (somebody trying to
			       hide a hack?) */
			    IsEncodedSlash(uri))
				return false;

			++uri;
		} else if (*uri == '/') {
			++uri;

			if (IsEncodedDot(uri))
				/* encoded dot after a slash - what's this client
				   trying to hide? */
				return false;

			if (*uri == '.') {
				++uri;

				if (IsEncodedDot(uri))
					/* encoded dot after a real dot - smells fishy */
					return false;

				if (*uri == 0 || *uri == '/')
					return false;

				if (*uri == '.')
					/* disallow two dots after a slash, even if
					   something else follows - this is the paranoid
					   function after all! */
					return false;
			}
		} else
			++uri;
	}

	return true;
}

bool
uri_path_verify_quick(const char *uri) noexcept
{
	if (*uri != '/')
		/* must begin with a slash */
		return false;

	for (++uri; *uri != 0; ++uri)
		if ((signed char)*uri <= 0x20)
			return false;

	return true;
}
