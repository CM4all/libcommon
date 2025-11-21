// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Base64.hxx"
#include "util/StringBuffer.hxx"

#include <cstddef>
#include <span>

template<int variant=sodium_base64_VARIANT_ORIGINAL, std::size_t src_size>
requires(src_size != std::dynamic_extent)
[[gnu::pure]]
auto
FixedBase64(std::span<const std::byte, src_size> src) noexcept
{
	StringBuffer<sodium_base64_ENCODED_LEN(src_size, variant)> dest;
	sodium_bin2base64(std::span{dest.data(), dest.capacity()},
			  src,
			  variant);
	return dest;
}

/* convenience overload for non-const spans */
template<int variant=sodium_base64_VARIANT_ORIGINAL, std::size_t src_size>
requires(src_size != std::dynamic_extent)
[[gnu::pure]]
auto
FixedBase64(std::span<std::byte, src_size> src) noexcept
{
	return FixedBase64<variant>(std::as_bytes(src));
}
