// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

class AllocatedString;

/**
 * @param escape_char the character that is used to escape; use '%'
 * for normal URIs
 */
std::size_t
UriEscape(char *dest, std::string_view src,
	  char escape_char='%') noexcept;

std::size_t
UriEscape(char *dest, std::span<const std::byte> src,
	  char escape_char='%') noexcept;

AllocatedString
UriEscape(std::string_view src, char escape_char='%') noexcept;

AllocatedString
UriEscape(std::span<const std::byte> src, char escape_char='%') noexcept;
