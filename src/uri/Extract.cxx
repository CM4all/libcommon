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

#include "Extract.hxx"
#include "util/CharUtil.hxx"
#include "util/StringSplit.hxx"

#include <algorithm>

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

	return std::all_of(std::next(p.begin()), p.end(), IsValidSchemeChar);
}

bool
UriHasScheme(const std::string_view uri) noexcept
{
	const auto [scheme, rest] = Split(uri, ':');
	return rest.data() != nullptr &&
		IsValidScheme(scheme) &&
		rest.starts_with("//"sv);
}

const char *
UriAfterScheme(const char *uri) noexcept
{
	return UriAfterScheme(std::string_view{uri}).data();
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
UriHostAndPort(const char *uri) noexcept
{
	assert(uri != nullptr);

	uri = UriAfterScheme(uri);
	if (uri == nullptr)
		return {};

	if (const auto [host_and_port, rest] = Split(std::string_view{uri}, '/');
	    rest.data() != nullptr)
		return host_and_port;

	return uri;
}

const char *
UriPathQueryFragment(const char *uri) noexcept
{
	assert(uri != nullptr);

	const char *ap = UriAfterScheme(uri);
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
