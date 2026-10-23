// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "util/VCircularBuffer.hxx"

#include <gtest/gtest.h>

#include <array>

TEST(VCircularBuffer, Basic)
{
	struct Foo {
		int value;

		Foo(int _value):value(_value) {}
	};

	std::array<std::byte, 4096> buffer;
	VCircularBuffer<Foo> cb{std::span{buffer}};
	ASSERT_TRUE(cb.empty());
	EXPECT_EQ(cb.size(), 0u);
	EXPECT_EQ(cb.begin(), cb.end());

	int i = 0;
	do {
		cb.emplace_back(sizeof(Foo), i++);
		ASSERT_FALSE(cb.empty());
		ASSERT_EQ(cb.back().value, i - 1);
	} while (cb.front().value == 0);

	EXPECT_EQ(cb.size(), size_t(i - 1));
	EXPECT_NE(cb.begin(), cb.end());
	EXPECT_EQ(&cb.front(), &*cb.begin());
	EXPECT_EQ(std::distance(cb.begin(), cb.end()), i - 1);

	i = cb.front().value;
	for (const auto &j : cb) {
		EXPECT_EQ(j.value, i);
		++i;
	}

	// const_iterator test
	const auto &cb_const = cb;
	i = cb_const.front().value;
	for (const auto &j : cb_const) {
		EXPECT_EQ(j.value, i);
		++i;
	}

	ASSERT_EQ(cb.front().value, 1);
	cb.emplace_back(sizeof(Foo), i++);
	ASSERT_EQ(cb.front().value, 2);
	cb.emplace_back(sizeof(Foo), i++);
	ASSERT_EQ(cb.front().value, 3);

	cb.emplace_back(1024, 10000);
	const unsigned n_deleted = cb.front().value - 1 - 3;
	ASSERT_GT(n_deleted, 1024 / (sizeof(Foo) + sizeof(void *) * 8));

	while (cb.front().value != 10000)
		cb.emplace_back(sizeof(Foo), i++);

	cb.emplace_back(sizeof(Foo), i++);
	const auto new_front = cb.front();
	for (unsigned n_added = 1; n_added < n_deleted; ++n_added) {
		cb.emplace_back(sizeof(Foo), i++);
		ASSERT_EQ(cb.front().value, new_front.value);
	}

	cb.emplace_back(sizeof(Foo), i++);

	if (sizeof(void*) < 8)
		/* dirty kludge: on 32 bit systems, we need another
		   item to displace the front item */
		cb.emplace_back(sizeof(Foo), i++);

	ASSERT_EQ(cb.front().value, new_front.value + 1);
}

/*
 * Regression test: the caller's span end need not be aligned to
 * alignof(Item).  AfterLast() rounds an item's end up to alignof(Item),
 * so with an unaligned end a tail item ending in the slack below the end
 * used to make AfterLast() point past the buffer; MakeFree() then
 * underflowed BytesDelta() and constructed the next item out of bounds.
 */
TEST(VCircularBuffer, MisalignedEnd)
{
	struct Foo {
		int value;

		Foo(int _value):value(_value) {}
	};

	/* for this small T, alignof(Item) is the alignment of the
	   intrusive list hook, i.e. a pointer */
	static constexpr size_t item_align = alignof(void *);

	alignas(item_align) std::array<std::byte, 2048> storage;

	for (size_t misalign = 1; misalign < item_align; ++misalign) {
		const std::span<std::byte> full{storage};

		/* trim the span so its end address is misaligned by
		   "misalign" bytes relative to alignof(Item) */
		const size_t end =
			reinterpret_cast<size_t>(full.data() + full.size());
		const size_t trim = (end - misalign) % item_align;
		const std::span<std::byte> buffer = full.first(full.size() - trim);

		ASSERT_EQ(reinterpret_cast<size_t>(buffer.data() + buffer.size()) % item_align,
			  misalign);

		VCircularBuffer<Foo> cb{buffer};

		const std::byte *const begin = buffer.data();
		const std::byte *const limit = buffer.data() + buffer.size();

		/* fill with items of varying size so the tail eventually
		   ends in the slack below the (misaligned) end; every new
		   item must land inside the buffer, never past the end */
		for (int i = 0; i < 4000; ++i) {
			const size_t value_size = sizeof(Foo) + (i % 32);
			auto &ref = cb.emplace_back(value_size, i);

			const auto *p = reinterpret_cast<const std::byte *>(&ref);
			ASSERT_GE(p, begin);
			ASSERT_LE(p + value_size, limit);
		}
	}
}
