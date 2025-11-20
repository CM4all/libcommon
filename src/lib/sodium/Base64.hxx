// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/StringBuffer.hxx"

#include <sodium/utils.h>

template<std::size_t src_size, int variant>
[[gnu::pure]]
auto
FixedBase64(const void *src) noexcept
{
	StringBuffer<sodium_base64_ENCODED_LEN(src_size, variant)> dest;
	sodium_bin2base64(dest.data(), dest.capacity(),
			  reinterpret_cast<const unsigned char *>(src), src_size,
			  variant);
	return dest;
}
