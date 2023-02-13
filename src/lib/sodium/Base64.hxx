// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/StringBuffer.hxx"

#include <sodium/utils.h>

#include <span>
#include <string_view>

class AllocatedString;
template<typename T> class AllocatedArray;

template<std::size_t src_size, int variant>
[[gnu::pure]]
auto
FixedBase64(const void *src) noexcept
{
	StringBuffer<sodium_base64_ENCODED_LEN(src_size, variant)> dest;
	sodium_bin2base64(dest.data(), dest.capacity(),
			  (const unsigned char *)src, src_size,
			  variant);
	return dest;
}

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
 * @return the decoded string or a nulled instance on error
 */
[[gnu::pure]]
AllocatedArray<std::byte>
DecodeUrlSafeBase64(std::string_view src) noexcept;
