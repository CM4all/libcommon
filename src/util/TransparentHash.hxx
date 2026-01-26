// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

/**
 * A hasher based on the DJB's cdb hash function.  Being
 * "transparent", it allows efficient standard container lookup.
 */
struct TransparentHash {
	using is_transparent = void;

	[[gnu::pure]]
	std::size_t operator()(const char *s) const noexcept;

	[[gnu::pure]]
	std::size_t operator()(std::span<const std::byte> s) const noexcept;

	[[gnu::pure]]
	std::size_t operator()(std::string_view s) const noexcept;
};
