/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

	int i = 0;
	do {
		cb.emplace_back(sizeof(Foo), i++);
		ASSERT_FALSE(cb.empty());
		ASSERT_EQ(cb.back().value, i - 1);
	} while (cb.front().value == 0);

	EXPECT_EQ(cb.size(), size_t(i - 1));

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
