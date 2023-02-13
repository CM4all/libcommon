// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

/**
 * @param escape_char the character that is used to escape; use '%'
 * for normal URIs
 * @return pointer to the end of the destination buffer (not
 * null-terminated) or nullptr on error
 */
char *
UriUnescape(char *dest, std::string_view src,
	    char escape_char='%') noexcept;
