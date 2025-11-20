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

/**
 * A wrapper for sodium_base642bin() that expects the decoded output
 * to be exactly the size of the destination buffer and that there is
 * no unparsed garbage at the end of the input string.
 */
static inline bool
StrictDecodeBase64(std::span<std::byte> bin,
		   std::string_view b64, int variant)
{
	std::size_t bin_len;
	const char *b64_end;
	return sodium_base642bin(bin, b64, nullptr, &bin_len,
				 &b64_end, variant) == 0 &&
		bin_len == bin.size() &&
		b64_end == b64.data() + b64.size();
}
