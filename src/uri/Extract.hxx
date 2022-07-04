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
