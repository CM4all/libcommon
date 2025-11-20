// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <sodium/utils.h>

#include <cstddef>
#include <span>
#include <string_view>

static inline void
sodium_bin2base64(std::span<char> b64,
		  std::span<const std::byte> bin,
		  int variant) noexcept
{
	sodium_bin2base64(b64.data(), b64.size(),
			  reinterpret_cast<const unsigned char *>(bin.data()), bin.size(),
			  variant);
}

static inline int
sodium_base642bin(std::span<std::byte> bin,
		  std::string_view b64,
		  const char *ignore,
		  std::size_t *bin_len,
		  const char **b64_end, int variant) noexcept
{
	return sodium_base642bin(reinterpret_cast<unsigned char *>(bin.data()), bin.size(),
				 b64.data(), b64.size(),
				 ignore, bin_len,
				 b64_end, variant);
}
