// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "djb_hash.hxx"

#include <cassert>

[[gnu::always_inline]]
static constexpr uint_fast32_t
djb_hash_update(uint_fast32_t hash, std::byte b) noexcept
{
	return (hash * 33) ^ static_cast<uint_fast8_t>(b);
}

uint32_t
djb_hash(std::span<const std::byte> src) noexcept
{
	uint_fast32_t hash = 5381;

	for (const auto i : src)
		hash = djb_hash_update(hash, i);

	return hash;
}

uint32_t
djb_hash_string(const char *p) noexcept
{
	assert(p != nullptr);

	uint_fast32_t hash = 5381;

	while (*p != 0)
		hash = djb_hash_update(hash, static_cast<std::byte>(*p++));

	return hash;
}
