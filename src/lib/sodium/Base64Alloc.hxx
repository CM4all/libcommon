// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/StringBuffer.hxx"

#include <sodium/utils.h>

#include <span>
#include <string_view>

class AllocatedString;
template<typename T> class AllocatedArray;

[[gnu::pure]]
AllocatedString
SodiumBase64(std::span<const std::byte> src,
	     int variant=sodium_base64_VARIANT_ORIGINAL) noexcept;

[[gnu::pure]]
AllocatedString
SodiumBase64(std::string_view src,
	     int variant=sodium_base64_VARIANT_ORIGINAL) noexcept;

[[gnu::pure]]
AllocatedString
UrlSafeBase64(std::span<const std::byte> src) noexcept;

[[gnu::pure]]
AllocatedString
UrlSafeBase64(std::string_view src) noexcept;

/**
 * @return the decoded string or a nulled instance on error
 */
[[gnu::pure]]
AllocatedArray<std::byte>
DecodeBase64(std::string_view src) noexcept;

/**
 * Like DecodeBase64(), but ignore space, tab and newline.
 */
[[gnu::pure]]
AllocatedArray<std::byte>
DecodeBase64IgnoreWhitespace(std::string_view src) noexcept;

/**
 * @return the decoded string or a nulled instance on error
 */
[[gnu::pure]]
AllocatedArray<std::byte>
DecodeUrlSafeBase64(std::string_view src) noexcept;
