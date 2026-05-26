// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "RegexPointer.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/SharedLease.hxx"

#include <string_view>

namespace Pcre {

class SharedRegex;

/**
 * A cache of precompiled #UniqueRegex instances.  Unused instances
 * will be evicted immediately.
 */
class Cache {
	struct Key {
		std::string_view pattern;
		int options;

		friend constexpr auto operator<=>(const Key &,
						  const Key &) noexcept = default;

		struct Hash {
			constexpr std::size_t operator()(const Key &key) const noexcept;
		};
	};

	struct Item;

	struct ItemGetKey {
		constexpr Key operator()(const Item &item) const noexcept;
	};

	/**
	 * Map #Key (pattern and options) to #Item.
	 */
	IntrusiveHashSet<Item, 1024,
			 IntrusiveHashSetOperators<Item, ItemGetKey,
						   Key::Hash, std::equal_to<Key>>> items;

public:
	[[nodiscard]]
	Cache() noexcept;

	~Cache() noexcept;

	SharedRegex Get(std::string_view pattern, int options=0);
};

} // namespace Pcre
