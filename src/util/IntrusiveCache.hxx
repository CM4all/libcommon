// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Concepts.hxx"
#include "IntrusiveHashSet.hxx"
#include "IntrusiveList.hxx"

#include <cassert>
#include <concepts>

template<typename Operators, typename Item>
concept IntrusiveCacheOperatorsConcept = IntrusiveHashSetOperatorsConcept<Operators, Item> && requires (const Operators &ops, const Item &c, Item *p) {
	{ ops.size_of(c) } noexcept -> std::same_as<std::size_t>;
	{ ops.disposer(p) } noexcept -> std::same_as<void>;
};

struct IntrusiveCacheHook {
	IntrusiveHashSetHook<> intrusive_cache_key_hook;
	IntrusiveListHook<> intrusive_cache_chronological_hook;
};

/**
 * For classes which embed #IntrusiveCacheHook as base class.
 */
template<typename T>
struct IntrusiveCacheBaseHookTraits {
	static constexpr T *Cast(IntrusiveCacheHook *node) noexcept {
		return static_cast<T *>(node);
	}

	static constexpr auto &ToHook(T &t) noexcept {
		return static_cast<IntrusiveCacheHook &>(t);
	}
};

/**
 * For classes which embed #IntrusiveCacheHook as member.
 */
template<auto member>
struct IntrusiveCacheMemberHookTraits {
	using T = MemberPointerContainerType<decltype(member)>;

	static constexpr T *Cast(IntrusiveCacheHook *node) noexcept {
		return &ContainerCast(*node, member);
	}

	static constexpr auto &ToHook(T &t) noexcept {
		return t.*member;
	}
};

/**
 * @param GetKey a function object which extracts the "key" part of an
 * item
 */
template<typename T,
	 std::regular_invocable<const T &> GetKey,
	 std::regular_invocable<std::invoke_result_t<GetKey, const T &>> Hash,
	 std::predicate<std::invoke_result_t<GetKey, const T &>,
			std::invoke_result_t<GetKey, const T &>> Equal,
	 std::regular_invocable<const T> SizeOf,
	 Disposer<T> Disposer>
struct IntrusiveCacheOperators {
	using hasher = Hash;
	using key_equal = Equal;

	[[no_unique_address]]
	Hash hash;

	[[no_unique_address]]
	Equal equal;

	[[no_unique_address]]
	GetKey get_key;

	[[no_unique_address]]
	SizeOf size_of;

	[[no_unique_address]]
	Disposer disposer;
};

/**
 * A simple LRU cache.  Items are allocated by the caller and are
 * "intrusive", i.e. they contain a structure that helps integrating
 * it in the container.
 *
 * @param table_size the size of the internal hash table
 */
template<typename T,
	 std::size_t table_size,
	 IntrusiveCacheOperatorsConcept<T> Operators,
	 typename HookTraits=IntrusiveCacheBaseHookTraits<T>>
class IntrusiveCache {
	[[no_unique_address]]
	Operators ops;

	const std::size_t max_size;
	std::size_t size = 0;

	struct ChronologicalHookTraits {
		template<typename>
		using Hook = IntrusiveListHook<>;

		static constexpr T *Cast(IntrusiveListNode *node) noexcept {
			auto *hook = IntrusiveListMemberHookTraits<&IntrusiveCacheHook::intrusive_cache_chronological_hook>::Cast(node);
			return HookTraits::Cast(hook);
		}

		static constexpr auto &ToHook(T &t) noexcept {
			auto &hook = HookTraits::ToHook(t);
			return hook.intrusive_cache_chronological_hook;
		}
	};

	using ChronologicalList = IntrusiveList<T, ChronologicalHookTraits>;

	ChronologicalList chronological_list;

	struct KeyHookTraits {
		template<typename>
		using Hook = IntrusiveHashSetHook<>;

		static constexpr T *Cast(IntrusiveHashSetHook<> *node) noexcept {
			auto *hook = IntrusiveHashSetMemberHookTraits<&IntrusiveCacheHook::intrusive_cache_key_hook>::Cast(node);
			return HookTraits::Cast(hook);
		}

		static constexpr auto &ToHook(T &t) noexcept {
			auto &hook = HookTraits::ToHook(t);
			return hook.intrusive_cache_key_hook;
		}
	};

	using KeyMap =
		IntrusiveHashSet<T, table_size, Operators, KeyHookTraits>;

	KeyMap key_map;

public:
	using hasher = typename KeyMap::hasher;
	using key_equal = typename KeyMap::key_equal;

	[[nodiscard]]
	explicit IntrusiveCache(std::size_t _max_size) noexcept
		:max_size(_max_size) {}

	~IntrusiveCache() noexcept {
		clear();
	}

	IntrusiveCache(const IntrusiveCache &) = delete;
	IntrusiveCache &operator=(const IntrusiveCache &) = delete;

	bool empty() const noexcept {
		return chronological_list.empty();
	}

	void clear() noexcept {
		key_map.clear();

		chronological_list.clear_and_dispose([this](T *item){
#ifndef NDEBUG
			assert(size >= ops.size_of(*item));
			size -= ops.size_of(*item);
#endif
			ops.disposer(item);
		});

#ifndef NDEBUG
		assert(size == 0);
#else
		size = 0;
#endif
	}

	std::size_t GetTotalSize() const noexcept {
		return size;
	}

	/**
	 * Look up an item by its key.  Returns nullptr if no such
	 * item exists.
	 */
	template<typename K>
	[[nodiscard]] [[gnu::pure]]
	T *Get(K &&key) noexcept {
		auto i = key_map.find(std::forward<K>(key));
		if (i == key_map.end())
			return nullptr;

		T &item = *i;
		assert(size >= ops.size_of(item));

		/* move to the front of the chronological list */
		chronological_list.erase(chronological_list.iterator_to(item));
		chronological_list.push_front(item);

		return &item;
	}

	/**
	 * Insert a new item into the cache.  The key must not exist
	 * already, i.e. Get() has returned nullptr; it is not
	 * possible to replace an existing item.  If the cache is
	 * full, then the least recently used item is deleted, making
	 * room for this one.
	 */
	void Put(T &item) {
		auto [it, inserted] = key_map.insert_check(ops.get_key(item));
		key_map.insert_commit(it, item);
		chronological_list.push_front(item);
		size += ops.size_of(item);

		if (!inserted)
			RemoveItem(*it);

		while (size > max_size)
			RemoveItem(GetOldest());
	}

	/**
	 * Remove an item from the cache using a reference to the
	 * value.
	 */
	void RemoveItem(T &item) noexcept {
		assert(size >= ops.size_of(item));

		key_map.erase(key_map.iterator_to(item));
		chronological_list.erase(chronological_list.iterator_to(item));
		size -= ops.size_of(item);
		ops.disposer(&item);
	}

	/**
	 * Remove an item from the cache.
	 */
	template<typename K>
	void Remove(K &&key) noexcept {
		T *item = Get(std::forward<K>(key));
		if (item != nullptr)
			RemoveItem(*item);
	}

	/**
	 * Iterates over all items and remove all those which match
	 * the given predicate.
	 */
	void RemoveIf(std::predicate<const T &> auto p) noexcept {
		chronological_list.remove_and_dispose_if(p, [this](T *item){
			assert(size >= ops.size_of(*item));

			key_map.erase(key_map.iterator_to(*item));
			size -= ops.size_of(*item);
			ops.disposer(item);
		});
	}

	/**
	 * Iterates over all items, passing each key/value pair to a
	 * given function.  The cache must not be modified from within
	 * that function.
	 */
	void ForEach(std::invocable<const T &> auto f) const {
		for (const auto &i : chronological_list)
			f(i);
	}

private:
	[[gnu::pure]]
	T &GetOldest() noexcept {
		assert(!chronological_list.empty());

		return chronological_list.back();
	}
};
