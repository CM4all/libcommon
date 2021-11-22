/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "lib/sodium/GenericHash.hxx"
#include "util/HexFormat.hxx"
#include "util/StringBuffer.hxx"

#include <gtest/gtest.h>

#include <array>
#include <cstddef> // for std::byte

template<std::size_t size>
static auto
HexFormat(const std::array<std::byte, size> &src) noexcept
{
	StringBuffer<size * 2 + 1> dest;
	for (size_t i = 0; i < src.size(); ++i)
		format_uint8_hex_fixed(&dest[i * 2], uint8_t(src[i]));
	dest[size * 2] = 0;
	return dest;
}

template<std::size_t bits>
static void
TGH(const char *expected)
{
	constexpr std::size_t size = bits / 8;
	static_assert(size * 8 == bits);
	GenericHashState state(size);
	const auto hash = HexFormat(state.GetFinalT<std::array<std::byte, size>>());
	EXPECT_STREQ(hash, expected);
}

TEST(TestGenericHash, Empty384)
{
	TGH<384>("b32811423377f52d7862286ee1a72ee540524380fda1724a6f25d7978c6fd3244a6caf0498812673c5e05ef583825100");
}

TEST(TestGenericHash, Empty512)
{
	TGH<512>("786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce");
}
