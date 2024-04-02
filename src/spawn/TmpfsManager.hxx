// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/IntrusiveList.hxx"

#include <string_view>
#include <utility> // for std::pair

class SharedLease;

/**
 * Manages a set of tmpfs instances for MOUNT_NAMED_TMPFS.
 *
 * Call Expire() periocally to delete expired instances.
 */
class TmpfsManager {
	struct Item;

	struct ItemHash {
		[[gnu::pure]]
		std::size_t operator()(std::string_view name) const noexcept;
	};

	struct ItemGetKey {
		[[gnu::pure]]
		std::string_view operator()(const Item &item) const noexcept;
	};

	IntrusiveHashSet<Item, 1024,
			 IntrusiveHashSetOperators<Item, ItemGetKey, ItemHash,
						   std::equal_to<std::string_view>>> items;
	IntrusiveList<Item> abandoned;

	const UniqueFileDescriptor mnt;

public:
	explicit TmpfsManager(UniqueFileDescriptor _mnt) noexcept;
	~TmpfsManager() noexcept;

	void Expire() noexcept;

	using Result = std::pair<UniqueFileDescriptor, SharedLease>;
	Result MakeTmpfs(std::string_view name, bool exec);
};
