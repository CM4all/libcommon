// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/sodium/SHA256.hxx"
#include "util/HexFormat.hxx"
#include "util/StringBuffer.hxx"

#include <gtest/gtest.h>

#include <array>

template<std::size_t size>
static auto
HexFormat(const std::array<std::byte, size> &src) noexcept
{
	StringBuffer<size * 2 + 1> dest;
	*HexFormat(dest.data(), std::span{src}) = 0;
	return dest;
}

TEST(TestSHA256, Empty)
{
	SHA256State state;
	const auto hash = HexFormat(state.Final());
	EXPECT_STREQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(TestSHA256, Empty1)
{
	const auto hash = HexFormat(SHA256(""));
	EXPECT_STREQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}
