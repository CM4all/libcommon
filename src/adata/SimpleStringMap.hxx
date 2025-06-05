// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/IntrusiveForwardList.hxx"
#include "util/StringAPI.hxx"
#include "util/TagStructs.hxx"

/**
 * A simple mapping with C string pointers as keys.  It is meant to be
 * used with items allocated from an #AllocatorPtr.
 */
template<typename T>
class SimpleStringMap {
public:
	struct Item : IntrusiveForwardListHook {
		const char *const key;
		T value;

		template<typename... Args>
		constexpr Item(const char *_key, Args&&... args) noexcept
			:key(_key), value(std::forward<Args>(args)...) {}

		constexpr Item(ShallowCopy shallow_copy, const Item &src) noexcept
			:key(src.key), value(shallow_copy, src.value) {}

		template<typename Alloc>
		constexpr Item(Alloc &&alloc, const Item &src) noexcept
			:key(alloc.Dup(src.key)),
			 value(alloc, src.value) {}
	};

private:
	IntrusiveForwardList<Item> list;

public:
	constexpr SimpleStringMap() noexcept = default;

	SimpleStringMap(ShallowCopy shallow_copy,
			const SimpleStringMap &src) noexcept
		:list(shallow_copy, src.list) {}

	template<typename Alloc>
	SimpleStringMap(Alloc &&alloc, const SimpleStringMap &src) noexcept {
		auto tail = list.before_begin();

		for (const auto &i : src.list) {
			auto *item = alloc.template New<Item>(alloc, i);
			tail = list.insert_after(tail, *item);
		}
	}

	SimpleStringMap(SimpleStringMap &&) = default;
	SimpleStringMap &operator=(SimpleStringMap &&) = default;

	constexpr bool empty() const noexcept {
		return list.empty();
	}

	constexpr void clear() noexcept {
		list.clear();
	}

	[[gnu::pure]]
	const T *Get(const char *key) const noexcept {
		for (const auto &i : list)
			if (StringIsEqual(key, i.key))
				return &i.value;

		return nullptr;
	}

	template<typename Alloc, typename... Args>
	T &Add(Alloc &&alloc, const char *key, Args&&... args) {
		auto *item = alloc.template New<Item>(key, std::forward<Args>(args)...);
		list.push_front(*item);
		return item->value;
	}
};
