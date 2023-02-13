// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Extract parts of an URI.
 */

#pragma once

#include <string_view>
#include <utility>

[[gnu::pure]]
bool
UriHasScheme(std::string_view uri) noexcept;

/**
 * Return the URI part after the protocol specification (and after the
 * double slash).
 */
[[gnu::pure]]
std::string_view
UriAfterScheme(std::string_view uri) noexcept;

/**
 * Does this URI have an authority part?
 */
[[gnu::pure]]
inline bool
UriHasAuthority(std::string_view uri) noexcept
{
	return UriAfterScheme(uri).data() != nullptr;
}

[[gnu::pure]]
std::string_view
UriHostAndPort(std::string_view uri) noexcept;

/**
 * Returns the URI path (including the query and the fragment) or
 * nullptr if the given URI has no path.
 */
[[gnu::pure]]
const char *
UriPathQueryFragment(const char *uri) noexcept;

[[gnu::pure]]
const char *
UriQuery(const char *uri) noexcept;
