// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/IntrusiveCache.hxx"
#include "util/DeleteDisposer.hxx"

#include <gtest/gtest.h>

TEST(IntrusiveCache, Basic)
{
	struct Counters {
		std::size_t n_constructed = 0, n_destructed = 0;
	};

	struct Item final : IntrusiveCacheHook {
		Counters &counters;

		int key;

		Item(Counters &_counters, int _key) noexcept
			:counters(_counters), key(_key)
		{
			++counters.n_constructed;
		}

		~Item() noexcept {
			++counters.n_destructed;
		}

		Item(const Item &) = delete;
		Item &operator=(const Item &) = delete;

		struct GetKey {
			constexpr int operator()(const Item &item) const noexcept {
				return item.key;
			}
		};

		struct GetSize {
			constexpr std::size_t operator()(const Item &item) const noexcept {
				return sizeof(item);
			}
		};
	};

	Counters counters;
	IntrusiveCache<Item, 2, IntrusiveCacheOperators<Item, Item::GetKey, std::hash<int>, std::equal_to<int>, Item::GetSize, DeleteDisposer>> cache{sizeof(Item) * 4};

	EXPECT_EQ(cache.Get(1), nullptr);
	EXPECT_EQ(cache.Get(2), nullptr);
	EXPECT_EQ(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(4), nullptr);
	EXPECT_EQ(cache.Get(5), nullptr);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);

	auto *item1 = new Item(counters, 1);
	cache.Put(*item1);
	EXPECT_EQ(cache.Get(1), item1);
	EXPECT_EQ(cache.Get(2), nullptr);
	EXPECT_EQ(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(4), nullptr);
	EXPECT_EQ(cache.Get(5), nullptr);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);
	EXPECT_EQ(counters.n_constructed, 1);
	EXPECT_EQ(counters.n_destructed, 0);

	auto *item2 = new Item(counters, 2);
	cache.Put(*item2);
	EXPECT_EQ(cache.Get(1), item1);
	EXPECT_EQ(cache.Get(2), item2);
	EXPECT_EQ(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(4), nullptr);
	EXPECT_EQ(cache.Get(5), nullptr);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);
	EXPECT_EQ(counters.n_constructed, 2);
	EXPECT_EQ(counters.n_destructed, 0);

	auto *item3 = new Item(counters, 3);
	cache.Put(*item3);
	EXPECT_EQ(cache.Get(1), item1);
	EXPECT_EQ(cache.Get(2), item2);
	EXPECT_EQ(cache.Get(3), item3);
	EXPECT_EQ(cache.Get(4), nullptr);
	EXPECT_EQ(cache.Get(5), nullptr);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);
	EXPECT_EQ(counters.n_constructed, 3);
	EXPECT_EQ(counters.n_destructed, 0);

	auto *item1b = new Item(counters, 1);
	cache.Put(*item1b);
	EXPECT_EQ(cache.Get(1), item1b);
	EXPECT_EQ(cache.Get(2), item2);
	EXPECT_EQ(cache.Get(3), item3);
	EXPECT_EQ(cache.Get(4), nullptr);
	EXPECT_EQ(cache.Get(5), nullptr);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);
	EXPECT_EQ(counters.n_constructed, 4);
	EXPECT_EQ(counters.n_destructed, 1);

	auto *item4 = new Item(counters, 4);
	cache.Put(*item4);
	EXPECT_EQ(cache.Get(2), item2);
	EXPECT_EQ(cache.Get(3), item3);
	EXPECT_EQ(cache.Get(4), item4);
	EXPECT_EQ(cache.Get(1), item1b);
	EXPECT_EQ(cache.Get(5), nullptr);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);
	EXPECT_EQ(counters.n_constructed, 5);
	EXPECT_EQ(counters.n_destructed, 1);

	auto *item5 = new Item(counters, 5);
	cache.Put(*item5);
	EXPECT_EQ(cache.Get(1), item1b);
	EXPECT_EQ(cache.Get(2), nullptr);
	EXPECT_EQ(cache.Get(3), item3);
	EXPECT_EQ(cache.Get(4), item4);
	EXPECT_EQ(cache.Get(5), item5);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);
	EXPECT_EQ(counters.n_constructed, 6);
	EXPECT_EQ(counters.n_destructed, 2);

	cache.RemoveItem(*item3);
	EXPECT_EQ(cache.Get(1), item1b);
	EXPECT_EQ(cache.Get(2), nullptr);
	EXPECT_EQ(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(4), item4);
	EXPECT_EQ(cache.Get(5), item5);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);
	EXPECT_EQ(counters.n_constructed, 6);
	EXPECT_EQ(counters.n_destructed, 3);

	cache.Remove(5);
	EXPECT_EQ(cache.Get(1), item1b);
	EXPECT_EQ(cache.Get(2), nullptr);
	EXPECT_EQ(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(4), item4);
	EXPECT_EQ(cache.Get(5), nullptr);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);
	EXPECT_EQ(counters.n_constructed, 6);
	EXPECT_EQ(counters.n_destructed, 4);

	cache.Remove(4711);
	EXPECT_EQ(cache.Get(1), item1b);
	EXPECT_EQ(cache.Get(2), nullptr);
	EXPECT_EQ(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(4), item4);
	EXPECT_EQ(cache.Get(5), nullptr);
	EXPECT_EQ(cache.Get(6), nullptr);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), nullptr);
	EXPECT_EQ(counters.n_constructed, 6);
	EXPECT_EQ(counters.n_destructed, 4);

	auto *item6 = new Item(counters, 6);
	cache.Put(*item6);
	auto *item7 = new Item(counters, 7);
	cache.Put(*item7);
	auto *item8 = new Item(counters, 8);
	cache.Put(*item8);
	EXPECT_EQ(counters.n_constructed, 9);
	EXPECT_EQ(counters.n_destructed, 5);

	cache.RemoveIf([](const Item &item){
		return item.key % 2 == 1;
	});

	EXPECT_EQ(cache.Get(1), nullptr);
	EXPECT_EQ(cache.Get(2), nullptr);
	EXPECT_EQ(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(4), item4);
	EXPECT_EQ(cache.Get(5), nullptr);
	EXPECT_EQ(cache.Get(6), item6);
	EXPECT_EQ(cache.Get(7), nullptr);
	EXPECT_EQ(cache.Get(8), item8);
	EXPECT_EQ(counters.n_constructed, 9);
	EXPECT_EQ(counters.n_destructed, 6);

	int sum = 0;
	cache.ForEach([&sum](const Item &item){
		sum += item.key;
	});

	EXPECT_EQ(sum, 18);

	cache.clear();
	EXPECT_EQ(counters.n_constructed, 9);
	EXPECT_EQ(counters.n_destructed, 9);
}

TEST(IntrusiveCache, MemberHook)
{
	struct Item final {
		IntrusiveCacheHook hook;

		int key;

		explicit Item(int _key) noexcept
			:key(_key) {}

		Item(const Item &) = delete;
		Item &operator=(const Item &) = delete;

		struct GetKey {
			constexpr int operator()(const Item &item) const noexcept {
				return item.key;
			}
		};

		struct GetSize {
			constexpr std::size_t operator()(const Item &item) const noexcept {
				return sizeof(item);
			}
		};
	};

	IntrusiveCache<Item, 2, IntrusiveCacheOperators<Item, Item::GetKey, std::hash<int>, std::equal_to<int>, Item::GetSize, DeleteDisposer>,
				IntrusiveCacheMemberHookTraits<&Item::hook>> cache{sizeof(Item) * 4};

	auto *item1 = new Item(1);
	cache.Put(*item1);
	EXPECT_EQ(cache.Get(1), item1);

	cache.RemoveItem(*item1);
	cache.Remove(2);

	cache.RemoveIf([](const Item &item){
		return item.key % 2 == 1;
	});

	int sum = 0;
	cache.ForEach([&sum](const Item &item){
		sum += item.key;
	});

	cache.clear();
}
