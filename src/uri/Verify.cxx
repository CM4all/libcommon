// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Verify.hxx"
#include "Chars.hxx"
#include "util/CharUtil.hxx"
#include "util/StringCompare.hxx"
#include "util/StringListVerify.hxx"
#include "util/StringSplit.hxx"
#include "util/StringVerify.hxx"

using std::string_view_literals::operator""sv;

static constexpr bool
IsAlphaNumericDashASCII(char ch) noexcept
{
    return IsAlphaNumericASCII(ch) || ch == '-';
}

static constexpr bool
IsLowerAlphaNumericDashASCII(char ch) noexcept
{
    return IsLowerAlphaNumericASCII(ch) || ch == '-';
}

bool
VerifyDomainLabel(std::string_view s) noexcept
{
	/* RFC 1035 2.3.4: domain labels are limited to 63 octets */
	if (s.empty() || s.size() > 63)
		return false;

	if (!IsAlphaNumericASCII(s.front()))
		return false;

	s.remove_prefix(1);
	if (s.empty())
		return true;

	if (!IsAlphaNumericASCII(s.back()))
		return false;

	s.remove_suffix(1);
	return CheckChars(s, IsAlphaNumericDashASCII);
}

/**
 * Like VerifyDomainLabel(), but don't allow upper case letters.
 */
bool
VerifyLowerDomainLabel(std::string_view s) noexcept
{
	/* RFC 1035 2.3.4: domain labels are limited to 63 octets */
	if (s.empty() || s.size() > 63)
		return false;

	if (!IsLowerAlphaNumericASCII(s.front()))
		return false;

	s.remove_prefix(1);
	if (s.empty())
		return true;

	if (!IsLowerAlphaNumericASCII(s.back()))
		return false;

	s.remove_suffix(1);
	return CheckChars(s, IsLowerAlphaNumericDashASCII);
}

bool
VerifyDomainName(std::string_view s) noexcept
{
	/* RFC 1035 2.3.4: domain names are limited to 255 octets */

	return s.size() <= 255 && IsNonEmptyListOf(s, '.', VerifyDomainLabel);
}

bool
VerifyLowerDomainName(std::string_view s) noexcept
{
	/* RFC 1035 2.3.4: domain names are limited to 255 octets */

	return s.size() <= 255 &&
		IsNonEmptyListOf(s, '.', VerifyLowerDomainLabel);
}

[[gnu::pure]]
static bool
VerifyPort(std::string_view s) noexcept
{
	return s.size() <= 5 &&
		CheckCharsNonEmpty(s, IsDigitASCII);
}

[[gnu::pure]]
static bool
VerifyIPv6Segment(std::string_view s) noexcept
{
	return s.size() <= 4 && CheckChars(s, IsHexDigit);
}

[[gnu::pure]]
static bool
VerifyIPv6(std::string_view host) noexcept
{
	return host.size() < 40 &&
		IsNonEmptyListOf(host, ':', VerifyIPv6Segment);
}

[[gnu::pure]]
static bool
VerifyUriHost(std::string_view host) noexcept
{
	if (host.find(':') != host.npos)
		return VerifyIPv6(host);

	return VerifyDomainName(host);
}

bool
VerifyUriHostPort(std::string_view host_port) noexcept
{
	if (host_port.empty())
		return false;

	if (host_port.front() == '[') {
		auto [host, port] = Split(host_port.substr(1), ']');
		if (port.data() == nullptr)
			/* syntax error: the closing bracket was not
			   found */
			return false;

		if (!port.empty()) {
			if (port.front() != ':')
				return false;

			port.remove_prefix(1);
			if (!VerifyPort(port))
				return false;
		}

		return VerifyUriHost(host);
	} else {
		auto [host, port] = SplitLast(host_port, ':');
		if (host.find(':') != host.npos)
			/* more than one colon: assume this is a
			   numeric IPv6 address (without a port
			   specification) */
			return VerifyIPv6(host_port);

		return VerifyUriHost(host) &&
			(port.data() == nullptr || VerifyPort(port));
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
uri_path_verify(std::string_view uri) noexcept
{
	if (uri.empty() || uri.front() != '/')
		/* path must begin with slash */
		return false;

	uri.remove_prefix(1); // strip the leading slash

	do {
		auto s = Split(uri, '/');
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

bool
VerifyUriQuery(std::string_view query) noexcept
{
	return CheckChars(query, IsUriQueryChar);
}

bool
VerifyHttpUrl(std::string_view url) noexcept
{
	if (!SkipPrefix(url, "http://"sv) && !SkipPrefix(url, "https://"sv))
		return false;

	const auto slash = url.find('/');
	if (slash == url.npos)
		return false;

	const auto [host_port, path_query] = Partition(url, slash);

	if (!VerifyUriHostPort(host_port))
		return false;

	const auto [path, query] = Split(path_query, '?');
	return uri_path_verify(path) && VerifyUriQuery(query);
}
