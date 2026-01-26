// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "TransparentHash.hxx"
#include "djb_hash.hxx"
#include "SpanCast.hxx"

std::size_t
TransparentHash::operator()(const char *s) const noexcept
{
	return djb_hash_string(s);
}

std::size_t
TransparentHash::operator()(std::span<const std::byte> s) const noexcept
{
	return djb_hash(s);
}

std::size_t
TransparentHash::operator()(std::string_view s) const noexcept
{
	return djb_hash(AsBytes(s));
}
