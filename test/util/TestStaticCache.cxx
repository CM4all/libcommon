// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/StaticCache.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(StaticCache, Basic)
{
	static unsigned n_constructed, n_destructed, n_overwritten;

	struct Value {
		int value;

		Value(int _value) noexcept
			:value(_value) {
			++n_constructed;
		}

		~Value() noexcept {
			++n_destructed;
		}

		Value(const Value &) = delete;
		Value &operator=(const Value &) = delete;

		Value &operator=(int src) noexcept {
			value = src;
			++n_overwritten;
			return *this;
		}
	};

	n_constructed = n_destructed = n_overwritten = 0;

	StaticCache<unsigned, Value, 8, 3> cache;
	EXPECT_TRUE(cache.IsEmpty());
	EXPECT_FALSE(cache.IsFull());
	EXPECT_EQ(cache.Get(1), nullptr);

	for (unsigned i = 1; i <= 7; ++i) {
		cache.Put(i, i);

		EXPECT_FALSE(cache.IsFull());
		EXPECT_EQ(n_constructed, i);
		EXPECT_EQ(n_destructed, 0U);
		EXPECT_EQ(n_overwritten, 0U);
	}

	cache.Put(8, 8);
	EXPECT_TRUE(cache.IsFull());
	EXPECT_EQ(n_constructed, 8U);
	EXPECT_EQ(n_destructed, 0U);
	EXPECT_EQ(n_overwritten, 0U);

	for (unsigned i = 1; i <= 8; ++i) {
		ASSERT_NE(cache.Get(i), nullptr);
		EXPECT_EQ(cache.Get(i)->value, i);
	}

	/* add one more item - this will evict the first one */

	cache.Put(9, 9);
	EXPECT_TRUE(cache.IsFull());
	EXPECT_EQ(n_constructed, 8U);
	EXPECT_EQ(n_destructed, 0U);
	EXPECT_EQ(n_overwritten, 1U);

	EXPECT_EQ(cache.Get(1), nullptr);

	for (unsigned i = 2; i <= 9; ++i) {
		ASSERT_NE(cache.Get(i), nullptr);
		EXPECT_EQ(cache.Get(i)->value, i);
	}

	/* add yet another item */

	cache.Put(10, 10);
	EXPECT_TRUE(cache.IsFull());
	EXPECT_EQ(n_constructed, 8U);
	EXPECT_EQ(n_destructed, 0U);
	EXPECT_EQ(n_overwritten, 2U);

	EXPECT_EQ(cache.Get(1), nullptr);
	EXPECT_EQ(cache.Get(2), nullptr);

	for (unsigned i = 3; i <= 10; ++i) {
		ASSERT_NE(cache.Get(i), nullptr);
		EXPECT_EQ(cache.Get(i)->value, i);
	}

	/* replace item */

	cache.PutOrReplace(3, 42);
	EXPECT_TRUE(cache.IsFull());
	EXPECT_EQ(n_constructed, 8U);
	EXPECT_EQ(n_destructed, 0U);
	EXPECT_EQ(n_overwritten, 3U);

	ASSERT_NE(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(3)->value, 42U);

	for (unsigned i = 4; i <= 10; ++i) {
		ASSERT_NE(cache.Get(i), nullptr);
		EXPECT_EQ(cache.Get(i)->value, i);
	}

	/* Remove() */

	cache.Remove(4);
	EXPECT_FALSE(cache.IsFull());
	EXPECT_EQ(n_constructed, 8U);
	EXPECT_EQ(n_destructed, 1U);
	EXPECT_EQ(n_overwritten, 3U);

	ASSERT_EQ(cache.Get(4), nullptr);
	ASSERT_NE(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(3)->value, 42U);

	for (unsigned i = 5; i <= 10; ++i) {
		ASSERT_NE(cache.Get(i), nullptr);
		EXPECT_EQ(cache.Get(i)->value, i);
	}

	/* RemoveIf() */

	cache.RemoveIf([](const auto &, const auto &value){
		return value.value < 8;
	});

	EXPECT_FALSE(cache.IsFull());
	EXPECT_EQ(n_constructed, 8U);
	EXPECT_EQ(n_destructed, 4U);
	EXPECT_EQ(n_overwritten, 3U);

	ASSERT_NE(cache.Get(3), nullptr);
	EXPECT_EQ(cache.Get(3)->value, 42U);

	for (unsigned i = 4; i <= 7; ++i)
		ASSERT_EQ(cache.Get(i), nullptr);

	for (unsigned i = 8; i <= 10; ++i) {
		ASSERT_NE(cache.Get(i), nullptr);
		EXPECT_EQ(cache.Get(i)->value, i);
	}

	/* refill */

	n_constructed = n_destructed = n_overwritten = 0;

	for (unsigned i = 11; i <= 13; ++i) {
		cache.Put(i, i);

		EXPECT_FALSE(cache.IsEmpty());
		EXPECT_FALSE(cache.IsFull());
		EXPECT_EQ(n_constructed, i - 10);
		EXPECT_EQ(n_destructed, 0U);
		EXPECT_EQ(n_overwritten, 0U);
	}

	cache.Put(14, 14);
	EXPECT_FALSE(cache.IsEmpty());
	EXPECT_TRUE(cache.IsFull());
	EXPECT_EQ(n_constructed, 4U);
	EXPECT_EQ(n_destructed, 0U);
	EXPECT_EQ(n_overwritten, 0U);

	cache.Put(15, 15);
	EXPECT_FALSE(cache.IsEmpty());
	EXPECT_TRUE(cache.IsFull());
	EXPECT_EQ(n_constructed, 4U);
	EXPECT_EQ(n_destructed, 0U);
	EXPECT_EQ(n_overwritten, 1U);

	/* clear */

	n_constructed = n_destructed = n_overwritten = 0;

	cache.Clear();
	EXPECT_TRUE(cache.IsEmpty());
	EXPECT_FALSE(cache.IsFull());
	EXPECT_EQ(n_constructed, 0U);
	EXPECT_EQ(n_destructed, 8U);
	EXPECT_EQ(n_overwritten, 0U);

	/* refill */

	n_constructed = n_destructed = n_overwritten = 0;

	for (unsigned i = 21; i <= 27; ++i) {
		cache.Put(i, i);

		EXPECT_FALSE(cache.IsEmpty());
		EXPECT_FALSE(cache.IsFull());
		EXPECT_EQ(n_constructed, i - 20);
		EXPECT_EQ(n_destructed, 0U);
		EXPECT_EQ(n_overwritten, 0U);
	}

	cache.Put(28, 28);
	EXPECT_FALSE(cache.IsEmpty());
	EXPECT_TRUE(cache.IsFull());
	EXPECT_EQ(n_constructed, 8U);
	EXPECT_EQ(n_destructed, 0U);
	EXPECT_EQ(n_overwritten, 0U);

	cache.Put(29, 29);
	EXPECT_FALSE(cache.IsEmpty());
	EXPECT_TRUE(cache.IsFull());
	EXPECT_EQ(n_constructed, 8U);
	EXPECT_EQ(n_destructed, 0U);
	EXPECT_EQ(n_overwritten, 1U);
}
