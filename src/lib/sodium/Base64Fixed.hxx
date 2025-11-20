// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Base64.hxx"
#include "util/StringBuffer.hxx"

#include <cstddef>

template<std::size_t src_size, int variant>
[[gnu::pure]]
auto
FixedBase64(const std::byte *src) noexcept
{
	StringBuffer<sodium_base64_ENCODED_LEN(src_size, variant)> dest;
	sodium_bin2base64(std::span{dest.data(), dest.capacity()},
			  {src, src_size},
			  variant);
	return dest;
}
