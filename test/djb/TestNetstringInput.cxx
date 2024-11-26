// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "net/djb/NetstringInput.hxx"
#include "net/SocketProtocolError.hxx"
#include "io/Pipe.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <array>

using std::string_view_literals::operator""sv;

static UniqueFileDescriptor
CreatePipeWithData(std::string_view src)
{
	auto [r, w] = CreatePipeNonBlock();
	w.FullWrite(AsBytes(src));
	return std::move(r);
}

TEST(NetstringInput, Empty)
{
	const auto fd = CreatePipeWithData("0:,"sv);
	NetstringInput ni{0};
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(fd), NetstringInput::Result::FINISHED);
	EXPECT_TRUE(ni.IsFinished());
	EXPECT_EQ(ToStringView(ni.GetValue()), ""sv);
}

TEST(NetstringInput, One)
{
	const auto fd = CreatePipeWithData("1:a,"sv);
	NetstringInput ni{1};
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(fd), NetstringInput::Result::FINISHED);
	EXPECT_TRUE(ni.IsFinished());
	EXPECT_EQ(ToStringView(ni.GetValue()), "a"sv);
}

TEST(NetstringInput, Two)
{
	const auto fd = CreatePipeWithData("2:ab,"sv);
	NetstringInput ni{2};
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(fd), NetstringInput::Result::FINISHED);
	EXPECT_TRUE(ni.IsFinished());
	EXPECT_EQ(ToStringView(ni.GetValue()), "ab"sv);
}

TEST(NetstringInput, TooLarge)
{
	const auto fd = CreatePipeWithData("2:ab,"sv);
	NetstringInput ni{1};
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_THROW(ni.Receive(fd), SocketMessageTooLargeError);
}

TEST(NetstringInput, TooLarge32)
{
	const auto fd = CreatePipeWithData("4294967296:"sv);
	NetstringInput ni{4294967295};
	EXPECT_FALSE(ni.IsFinished());

	if constexpr (sizeof(std::size_t) > 4)
		EXPECT_THROW(ni.Receive(fd), SocketMessageTooLargeError);
	else
		EXPECT_THROW(ni.Receive(fd), SocketProtocolError);
}

TEST(NetstringInput, TooLarge64)
{
	const auto fd = CreatePipeWithData("18446744073709551616:"sv);
	NetstringInput ni{4294967295};
	EXPECT_FALSE(ni.IsFinished());

	if constexpr (sizeof(std::size_t) > 8)
		EXPECT_THROW(ni.Receive(fd), SocketMessageTooLargeError);
	else
		EXPECT_THROW(ni.Receive(fd), SocketProtocolError);
}

TEST(NetstringInput, Malformed)
{
	static constexpr std::array tests{
		":x,"sv,
		"a:"sv,
		"0x1:z,"sv,
		"-1:"sv,
		"-2147483649:"sv,
		"00000000000000000000000000000000"sv,
		"1.0:z,"sv,
		"1e0:z,"sv,
		"0:x"sv,
		"1:xy"sv,
		"1:xy"sv,
		"2:xy,,"sv,
		"2:xy:"sv,
		"0:,0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"sv,
	};

	for (const std::string_view i : tests) {
		const auto fd = CreatePipeWithData(i);
		NetstringInput ni{4294967295};
		EXPECT_FALSE(ni.IsFinished());
		EXPECT_THROW(ni.Receive(fd), SocketProtocolError);
	}
}

TEST(NetstringInput, NoInput)
{
	const auto fd = CreatePipeWithData(""sv);
	NetstringInput ni{16384};
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(fd), NetstringInput::Result::CLOSED);
	EXPECT_FALSE(ni.IsFinished());
}

TEST(NetstringInput, ClosedPrematurely)
{
	static constexpr std::string_view tests[] = {
		"0"sv,
		"0:"sv,
		"1"sv,
		"1:"sv,
		"1:a"sv,
	};

	for (const std::string_view i : tests) {
		const auto fd = CreatePipeWithData(i);
		NetstringInput ni{16384};
		EXPECT_FALSE(ni.IsFinished());
		EXPECT_EQ(ni.Receive(fd), NetstringInput::Result::MORE);
		EXPECT_FALSE(ni.IsFinished());
		EXPECT_EQ(ni.Receive(fd), NetstringInput::Result::CLOSED);
		EXPECT_FALSE(ni.IsFinished());
	}
}

/**
 * Longer than header_buffer.
 */
TEST(NetstringInput, Long)
{
	const auto fd = CreatePipeWithData("62:0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz,"sv);
	NetstringInput ni{16384};
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(fd), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(fd), NetstringInput::Result::FINISHED);
	EXPECT_TRUE(ni.IsFinished());
	EXPECT_EQ(ToStringView(ni.GetValue()), "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"sv);
}

TEST(NetstringInput, Incremental)
{
	auto [r, w] = CreatePipeNonBlock();

	NetstringInput ni{16384};
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("16:0123"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("456789abcd"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("ef"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes(","sv));
	w.Close();

	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::FINISHED);
	EXPECT_TRUE(ni.IsFinished());
	EXPECT_EQ(ToStringView(ni.GetValue()), "0123456789abcdef"sv);
}

TEST(NetstringInput, IncrementalHeader)
{
	auto [r, w] = CreatePipeNonBlock();

	NetstringInput ni{16384};
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("1"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("6"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes(":"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("0123"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("456789abcd"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("ef"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes(","sv));
	w.Close();

	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::FINISHED);
	EXPECT_TRUE(ni.IsFinished());
	EXPECT_EQ(ToStringView(ni.GetValue()), "0123456789abcdef"sv);
}

TEST(NetstringInput, LongIncremental)
{
	auto [r, w] = CreatePipeNonBlock();

	NetstringInput ni{16384};
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("62:0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"sv));
	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::MORE);
	EXPECT_FALSE(ni.IsFinished());

	w.FullWrite(AsBytes("abcdefghijklmnopqrstuvwxyz,"sv));
	w.Close();

	EXPECT_EQ(ni.Receive(r), NetstringInput::Result::FINISHED);
	EXPECT_TRUE(ni.IsFinished());
	EXPECT_EQ(ToStringView(ni.GetValue()), "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"sv);
}
