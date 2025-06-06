// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lib/sodium/GenericHash.hxx"
#include "util/HexFormat.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <array>
#include <cstddef> // for std::byte

template<std::size_t bits>
static void
TGH(std::string_view expected)
{
	constexpr std::size_t size = bits / 8;
	static_assert(size * 8 == bits);
	GenericHashState state(size);
	const auto hash = state.GetFinalT<std::array<std::byte, size>>();
	const auto hex = HexFormat(std::span{hash});
	EXPECT_EQ(ToStringView(hex), expected);
}

TEST(TestGenericHash, Empty384)
{
	TGH<384>("b32811423377f52d7862286ee1a72ee540524380fda1724a6f25d7978c6fd3244a6caf0498812673c5e05ef583825100");
}

TEST(TestGenericHash, Empty512)
{
	TGH<512>("786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce");
}
