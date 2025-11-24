// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lib/sodium/SHA256.hxx"
#include "util/HexFormat.hxx"

#include <gtest/gtest.h>

#include <array>

using std::string_view_literals::operator""sv;

TEST(TestSHA256, Empty)
{
	SHA256State state;
	const auto hash = state.Final();
	const auto hex = HexFormat(std::span{hash});
	EXPECT_EQ(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"sv);
}

TEST(TestSHA256, Empty1)
{
	const auto hash = SHA256({});
	const auto hex = HexFormat(std::span{hash});
	EXPECT_EQ(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"sv);
}
