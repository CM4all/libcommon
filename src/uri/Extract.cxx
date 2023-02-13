// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Extract.hxx"
#include "util/CharUtil.hxx"
#include "util/StringSplit.hxx"
#include "util/StringVerify.hxx"

#include <assert.h>
#include <string.h>

using std::string_view_literals::operator""sv;

static constexpr bool
IsValidSchemeStart(char ch) noexcept
{
	return IsLowerAlphaASCII(ch);
}

static constexpr bool
IsValidSchemeChar(char ch) noexcept
{
	return IsLowerAlphaASCII(ch) || IsDigitASCII(ch) ||
		ch == '+' || ch == '.' || ch == '-';
}

[[gnu::pure]]
static bool
IsValidScheme(std::string_view p) noexcept
{
	if (p.empty() || !IsValidSchemeStart(p.front()))
		return false;

	return CheckChars(p.substr(1), IsValidSchemeChar);
}

bool
UriHasScheme(const std::string_view uri) noexcept
{
	const auto [scheme, rest] = Split(uri, ':');
	return rest.data() != nullptr &&
		IsValidScheme(scheme) &&
		rest.starts_with("//"sv);
}

std::string_view
UriAfterScheme(std::string_view uri) noexcept
{
	if (uri.size() > 2 && uri[0] == '/' && uri[1] == '/' && uri[2] != '/')
		return uri.substr(2);

	const auto [scheme, rest] = Split(uri, ':');
	if (IsValidScheme(scheme) &&
	    rest.size() > 2 && rest[0] == '/' && rest[1] == '/' &&
	    rest[2] != '/')
		return rest.substr(2);

	return {};
}

std::string_view
UriHostAndPort(std::string_view _uri) noexcept
{
	auto uri = UriAfterScheme(_uri);
	if (uri.data() == nullptr)
		return {};

	if (const auto [host_and_port, rest] = Split(uri, '/');
	    rest.data() != nullptr)
		return host_and_port;

	return uri;
}

const char *
UriPathQueryFragment(const char *uri) noexcept
{
	assert(uri != nullptr);

	const char *ap = UriAfterScheme(uri).data();
	if (ap != nullptr)
		return strchr(ap, '/');

	return uri;
}

const char *
UriQuery(const char *uri) noexcept
{
	assert(uri != nullptr);

	const char *p = strchr(uri, '?');
	if (p == nullptr || *++p == 0)
		return nullptr;

	return p;
}
