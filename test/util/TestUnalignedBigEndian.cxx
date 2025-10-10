// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/UnalignedBigEndian.hxx"

#include <gtest/gtest.h>

#include <array>

TEST(UnalignedBigEndian, BE16)
{
	static constexpr uint_least16_t value = 0x1234;
	static constexpr std::array expected{std::byte{0x12}, std::byte{0x34}};
	EXPECT_EQ(ReadUnalignedBE16(expected), value);

	std::array<std::byte, 2> buffer;
	EXPECT_EQ(WriteUnalignedBE16(buffer.data(), value), buffer.data() + buffer.size());
	EXPECT_EQ(buffer, expected);
}

TEST(UnalignedBigEndian, BE32)
{
	static constexpr uint_least32_t value = 0x12345678UL;
	static constexpr std::array expected{std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78}};
	EXPECT_EQ(ReadUnalignedBE32(expected), value);

	std::array<std::byte, 4> buffer;
	EXPECT_EQ(WriteUnalignedBE32(buffer.data(), value), buffer.data() + buffer.size());
	EXPECT_EQ(buffer, expected);
}

TEST(UnalignedBigEndian, BE64)
{
	static constexpr uint_least64_t value = 0x123456789abcdef0ULL;
	static constexpr std::array expected{
		std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78},
		std::byte{0x9a}, std::byte{0xbc}, std::byte{0xde}, std::byte{0xf0},
	};
	EXPECT_EQ(ReadUnalignedBE64(expected), value);

	std::array<std::byte, 8> buffer;
	EXPECT_EQ(WriteUnalignedBE64(buffer.data(), value), buffer.data() + buffer.size());
	EXPECT_EQ(buffer, expected);
}
