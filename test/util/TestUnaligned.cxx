// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/Unaligned.hxx"
#include "util/ByteOrder.hxx"

#include <gtest/gtest.h>

#include <array>

TEST(Unaligned, Unaligned)
{
	struct Foo {
		// this member defines the alignment of this struct
		uint64_t dummy;

		std::array<std::byte, 9> buffer;
	} foo;

	std::fill(foo.buffer.begin(), foo.buffer.end(), std::byte{0xff});
	EXPECT_EQ(LoadUnaligned<uint64_t>(foo.buffer.data() + 1),
		  0xffffffffffffffffULL);

	constexpr uint64_t token = 0xc0ffeedeadbeef;

	StoreUnaligned(foo.buffer.data() + 1, token);
	EXPECT_EQ(LoadUnaligned<uint64_t>(foo.buffer.data() + 1),
		  token);

	std::fill(foo.buffer.begin(), foo.buffer.end(), std::byte{0xff});
	EXPECT_EQ(LoadUnaligned<uint64_t>(foo.buffer.data() + 1),
		  0xffffffffffffffffULL);
	EXPECT_EQ(foo.buffer[1], std::byte{0xff});
	EXPECT_EQ(foo.buffer[2], std::byte{0xff});
	EXPECT_EQ(foo.buffer[3], std::byte{0xff});
	EXPECT_EQ(foo.buffer[4], std::byte{0xff});
	EXPECT_EQ(foo.buffer[5], std::byte{0xff});
	EXPECT_EQ(foo.buffer[6], std::byte{0xff});
	EXPECT_EQ(foo.buffer[7], std::byte{0xff});
	EXPECT_EQ(foo.buffer[8], std::byte{0xff});

	StoreUnaligned(foo.buffer.data() + 1, ToBE64(token));
	EXPECT_EQ(foo.buffer[1], std::byte{0x00});
	EXPECT_EQ(foo.buffer[2], std::byte{0xc0});
	EXPECT_EQ(foo.buffer[3], std::byte{0xff});
	EXPECT_EQ(foo.buffer[4], std::byte{0xee});
	EXPECT_EQ(foo.buffer[5], std::byte{0xde});
	EXPECT_EQ(foo.buffer[6], std::byte{0xad});
	EXPECT_EQ(foo.buffer[7], std::byte{0xbe});
	EXPECT_EQ(foo.buffer[8], std::byte{0xef});
	EXPECT_EQ(LoadUnaligned<uint64_t>(foo.buffer.data() + 1),
		  ToBE64(token));
	EXPECT_EQ(LoadUnaligned<uint32_t>(foo.buffer.data() + 1),
		  ToBE32(static_cast<uint32_t>(token >> 32)));
	EXPECT_EQ(LoadUnaligned<uint32_t>(foo.buffer.data() + 5),
		  ToBE32(static_cast<uint32_t>(token)));
}
