// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "util/HexParse.hxx"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>

TEST(ParseHexDigit, Valid)
{
	EXPECT_EQ(ParseHexDigit('0'), 0);
	EXPECT_EQ(ParseHexDigit('1'), 1);
	EXPECT_EQ(ParseHexDigit('9'), 9);
	EXPECT_EQ(ParseHexDigit('a'), 0xa);
	EXPECT_EQ(ParseHexDigit('b'), 0xb);
	EXPECT_EQ(ParseHexDigit('f'), 0xf);
	EXPECT_EQ(ParseHexDigit('A'), 0xa);
	EXPECT_EQ(ParseHexDigit('B'), 0xb);
	EXPECT_EQ(ParseHexDigit('F'), 0xf);
}

TEST(ParseHexDigit, Invalid)
{
	EXPECT_EQ(ParseHexDigit('g'), -1);
	EXPECT_EQ(ParseHexDigit('G'), -1);
	EXPECT_EQ(ParseHexDigit('z'), -1);
	EXPECT_EQ(ParseHexDigit('Z'), -1);
	EXPECT_EQ(ParseHexDigit(' '), -1);
	EXPECT_EQ(ParseHexDigit('@'), -1);
	EXPECT_EQ(ParseHexDigit('/'), -1);
	EXPECT_EQ(ParseHexDigit(':'), -1);
}

TEST(ParseLowerHexDigit, Valid)
{
	EXPECT_EQ(ParseLowerHexDigit('0'), 0);
	EXPECT_EQ(ParseLowerHexDigit('1'), 1);
	EXPECT_EQ(ParseLowerHexDigit('9'), 9);
	EXPECT_EQ(ParseLowerHexDigit('a'), 0xa);
	EXPECT_EQ(ParseLowerHexDigit('b'), 0xb);
	EXPECT_EQ(ParseLowerHexDigit('f'), 0xf);
}

TEST(ParseLowerHexDigit, Invalid)
{
	EXPECT_EQ(ParseLowerHexDigit('A'), -1);
	EXPECT_EQ(ParseLowerHexDigit('B'), -1);
	EXPECT_EQ(ParseLowerHexDigit('F'), -1);
	EXPECT_EQ(ParseLowerHexDigit('g'), -1);
	EXPECT_EQ(ParseLowerHexDigit('z'), -1);
	EXPECT_EQ(ParseLowerHexDigit(' '), -1);
	EXPECT_EQ(ParseLowerHexDigit('@'), -1);
}

TEST(ParseLowerHexFixed, UInt8)
{
	uint8_t value;

	EXPECT_NE(ParseLowerHexFixed("00", value), nullptr);
	EXPECT_EQ(value, 0x00);

	EXPECT_NE(ParseLowerHexFixed("ff", value), nullptr);
	EXPECT_EQ(value, 0xff);

	EXPECT_NE(ParseLowerHexFixed("a5", value), nullptr);
	EXPECT_EQ(value, 0xa5);

	EXPECT_NE(ParseLowerHexFixed("12", value), nullptr);
	EXPECT_EQ(value, 0x12);

	EXPECT_EQ(ParseLowerHexFixed("", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("1", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("FF", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("gg", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("1g", value), nullptr);
}

TEST(ParseLowerHexFixed, UInt16)
{
	uint16_t value;

	EXPECT_NE(ParseLowerHexFixed("0000", value), nullptr);
	EXPECT_EQ(value, 0x0000);

	EXPECT_NE(ParseLowerHexFixed("ffff", value), nullptr);
	EXPECT_EQ(value, 0xffff);

	EXPECT_NE(ParseLowerHexFixed("1234", value), nullptr);
	EXPECT_EQ(value, 0x1234);

	EXPECT_NE(ParseLowerHexFixed("abcd", value), nullptr);
	EXPECT_EQ(value, 0xabcd);

	EXPECT_EQ(ParseLowerHexFixed("123", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("ABCD", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("12gg", value), nullptr);

	const char *too_long = "12345";
	EXPECT_EQ(ParseLowerHexFixed(too_long, value), too_long + 4);
	EXPECT_EQ(value, 0x1234);
}

TEST(ParseLowerHexFixed, UInt32)
{
	uint32_t value;

	EXPECT_NE(ParseLowerHexFixed("00000000", value), nullptr);
	EXPECT_EQ(value, 0x00000000);

	EXPECT_NE(ParseLowerHexFixed("ffffffff", value), nullptr);
	EXPECT_EQ(value, 0xffffffff);

	EXPECT_NE(ParseLowerHexFixed("12345678", value), nullptr);
	EXPECT_EQ(value, 0x12345678);

	EXPECT_NE(ParseLowerHexFixed("abcdef01", value), nullptr);
	EXPECT_EQ(value, 0xabcdef01);

	EXPECT_EQ(ParseLowerHexFixed("1234567", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("ABCDEF01", value), nullptr);

	const char *too_long = "123456789";
	EXPECT_EQ(ParseLowerHexFixed(too_long, value), too_long + 8);
	EXPECT_EQ(value, 0x12345678);
}

TEST(ParseLowerHexFixed, Byte)
{
	std::byte value;

	EXPECT_NE(ParseLowerHexFixed("00", value), nullptr);
	EXPECT_EQ(value, std::byte{0x00});

	EXPECT_NE(ParseLowerHexFixed("ff", value), nullptr);
	EXPECT_EQ(value, std::byte{0xff});

	EXPECT_NE(ParseLowerHexFixed("a5", value), nullptr);
	EXPECT_EQ(value, std::byte{0xa5});

	EXPECT_EQ(ParseLowerHexFixed("FF", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("gg", value), nullptr);
}

TEST(ParseLowerHexFixed, Array)
{
	std::array<uint8_t, 3> value;

	EXPECT_NE(ParseLowerHexFixed("00ffaa", value), nullptr);
	EXPECT_EQ(value[0], 0x00);
	EXPECT_EQ(value[1], 0xff);
	EXPECT_EQ(value[2], 0xaa);

	EXPECT_NE(ParseLowerHexFixed("123456", value), nullptr);
	EXPECT_EQ(value[0], 0x12);
	EXPECT_EQ(value[1], 0x34);
	EXPECT_EQ(value[2], 0x56);

	EXPECT_EQ(ParseLowerHexFixed("12345", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("12345G", value), nullptr);

	const char *too_long = "2345678";
	EXPECT_EQ(ParseLowerHexFixed(too_long, value), too_long + 6);
	EXPECT_EQ(value[0], 0x23);
	EXPECT_EQ(value[1], 0x45);
	EXPECT_EQ(value[2], 0x67);
}

TEST(ParseLowerHexFixed, Span)
{
	std::array<uint8_t, 3> array;
	std::span<uint8_t, 3> value{array};

	EXPECT_NE(ParseLowerHexFixed("00ffaa", value), nullptr);
	EXPECT_EQ(value[0], 0x00);
	EXPECT_EQ(value[1], 0xff);
	EXPECT_EQ(value[2], 0xaa);

	EXPECT_NE(ParseLowerHexFixed("123456", value), nullptr);
	EXPECT_EQ(value[0], 0x12);
	EXPECT_EQ(value[1], 0x34);
	EXPECT_EQ(value[2], 0x56);

	EXPECT_EQ(ParseLowerHexFixed("12345", value), nullptr);
	EXPECT_EQ(ParseLowerHexFixed("12345G", value), nullptr);

	const char *too_long = "2345678";
	EXPECT_EQ(ParseLowerHexFixed(too_long, value), too_long + 6);
	EXPECT_EQ(value[0], 0x23);
	EXPECT_EQ(value[1], 0x45);
	EXPECT_EQ(value[2], 0x67);
}

TEST(ParseLowerHexFixed, StringView)
{
	// initialize with bogus value to suppress -Wmaybe-uninitialized
	uint8_t value8{};
	uint16_t value16{};
	uint32_t value32{};

	EXPECT_TRUE(ParseLowerHexFixed(std::string_view("ab"), value8));
	EXPECT_EQ(value8, 0xab);

	EXPECT_TRUE(ParseLowerHexFixed(std::string_view("1234"), value16));
	EXPECT_EQ(value16, 0x1234);

	EXPECT_TRUE(ParseLowerHexFixed(std::string_view("deadbeef"), value32));
	EXPECT_EQ(value32, 0xdeadbeef);

	EXPECT_FALSE(ParseLowerHexFixed(std::string_view("a"), value8));
	EXPECT_FALSE(ParseLowerHexFixed(std::string_view("abc"), value8));
	EXPECT_FALSE(ParseLowerHexFixed(std::string_view("AB"), value8));
	EXPECT_FALSE(ParseLowerHexFixed(std::string_view("12345"), value16));
	EXPECT_FALSE(ParseLowerHexFixed(std::string_view("123"), value16));
	EXPECT_FALSE(ParseLowerHexFixed(std::string_view("DEADBEEF"), value32));
	EXPECT_FALSE(ParseLowerHexFixed(std::string_view("deadbee"), value32));
}
