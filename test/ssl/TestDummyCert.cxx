/*
 * Copyright 2019-2022 CM4all GmbH
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

#include "lib/openssl/Dummy.hxx"
#include "lib/openssl/Key.hxx"
#include "util/HexFormat.hxx"

#include <gtest/gtest.h>

TEST(TestDummyCert, RSA)
{
	const auto key1 = GenerateRsaKey();
	const auto cert1 = MakeSelfSignedDummyCert(*key1, "foo");

	const auto key2 = GenerateRsaKey();
	const auto cert2 = MakeSelfSignedDummyCert(*key2, "foo");

	EXPECT_TRUE(MatchModulus(*cert1, *key1));
	EXPECT_TRUE(MatchModulus(*cert2, *key2));
	EXPECT_FALSE(MatchModulus(*cert1, *key2));
	EXPECT_FALSE(MatchModulus(*cert2, *key1));
}

TEST(TestDummyCert, EC)
{
	const auto key1 = GenerateEcKey();
	const auto cert1 = MakeSelfSignedDummyCert(*key1, "foo");

	const auto key2 = GenerateEcKey();
	const auto cert2 = MakeSelfSignedDummyCert(*key2, "foo");

	EXPECT_TRUE(MatchModulus(*cert1, *key1));
	EXPECT_TRUE(MatchModulus(*cert2, *key2));
	EXPECT_FALSE(MatchModulus(*cert1, *key2));
	EXPECT_FALSE(MatchModulus(*cert2, *key1));
}
