/*
 * Copyright 2007-2017 Content Management AG
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

#include "util/FNVHash.hxx"

#include <gtest/gtest.h>

TEST(FNVHash, u32)
{
	EXPECT_EQ(FNV1aHash32(""), 2166136261u);

	/* zero-hash challenge results from
	   http://www.isthe.com/chongo/tech/comp/fnv/ */
	EXPECT_EQ(FNV1aHash32("\xcc\x24\x31\xc4"), 0u);
	EXPECT_EQ(FNV1aHash32("\xe0\x4d\x9f\xcb"), 0u);

	/* from IETF draft-eastlake-fnv Appendix C: A Few Test Vectors */
	EXPECT_EQ(FNV1aHash32("a"), 0xe40c292c);
	EXPECT_EQ(FNV1aHash32("foobar"), 0xbf9cf968);
}

TEST(FNVHash, u32Binary)
{
	EXPECT_EQ(FNV1aHash32(ConstBuffer<void>{nullptr}), 2166136261u);

	static constexpr uint8_t zero_hash_challenge1[] = {0xcc, 0x24, 0x31, 0xc4};
	EXPECT_EQ(FNV1aHash32(ConstBuffer<uint8_t>{zero_hash_challenge1}.ToVoid()), 0u);
}

TEST(FNVHash, u64)
{
	EXPECT_EQ(FNV1aHash64(""), 14695981039346656037u);

	/* zero-hash challenge results from
	   http://www.isthe.com/chongo/tech/comp/fnv/ */
	EXPECT_EQ(FNV1aHash64("\xd5\x6b\xb9\x53\x42\x87\x08\x36"), 0u);

	/* from IETF draft-eastlake-fnv Appendix C: A Few Test Vectors */
	EXPECT_EQ(FNV1aHash64("a"), 0xaf63dc4c8601ec8c);
	EXPECT_EQ(FNV1aHash64("foobar"), 0x85944171f73967e8);
}

TEST(FNVHash, u64Binary)
{
	EXPECT_EQ(FNV1aHash64(ConstBuffer<void>{nullptr}), 14695981039346656037u);

	static constexpr uint8_t zero_hash_challenge1[] = {0xd5, 0x6b, 0xb9, 0x53, 0x42, 0x87, 0x08, 0x36};
	EXPECT_EQ(FNV1aHash64(ConstBuffer<uint8_t>{zero_hash_challenge1}.ToVoid()), 0u);
}
