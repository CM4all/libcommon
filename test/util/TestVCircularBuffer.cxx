// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
	ASSERT_EQ(cb.front().value, new_front.value + 1);
}
