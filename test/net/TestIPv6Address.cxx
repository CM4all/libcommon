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

#include "net/IPv6Address.hxx"

#include <gtest/gtest.h>

TEST(IPv6AddressTest, Basic)
{
	IPv6Address dummy;
	EXPECT_EQ(dummy.GetSize(), sizeof(struct sockaddr_in6));
}

TEST(IPv6AddressTest, Port)
{
	IPv6Address a(12345);
	EXPECT_EQ(a.GetPort(), 12345u);

	a.SetPort(42);
	EXPECT_EQ(a.GetPort(), 42u);
}

static bool
operator==(const struct in6_addr &a, const struct in6_addr &b)
{
	return memcmp(&a, &b, sizeof(a)) == 0;
}

TEST(IPv6AddressTest, Mask)
{
	EXPECT_EQ(IPv6Address::MaskFromPrefix(0).GetAddress(),
		  IPv6Address(0, 0, 0, 0, 0, 0, 0, 0, 0).GetAddress());
	EXPECT_EQ(IPv6Address::MaskFromPrefix(128).GetAddress(),
		  IPv6Address(0xffff, 0xffff, 0xffff, 0xffff,
			      0xffff, 0xffff, 0xffff, 0xffff, 0).GetAddress());
	EXPECT_EQ(IPv6Address::MaskFromPrefix(127).GetAddress(),
		  IPv6Address(0xffff, 0xffff, 0xffff, 0xffff,
			      0xffff, 0xffff, 0xffff, 0xfffe, 0).GetAddress());
	EXPECT_EQ(IPv6Address::MaskFromPrefix(64).GetAddress(),
		  IPv6Address(0xffff, 0xffff, 0xffff, 0xffff,
			      0, 0, 0, 0, 0).GetAddress());
	EXPECT_EQ(IPv6Address::MaskFromPrefix(56).GetAddress(),
		  IPv6Address(0xffff, 0xffff, 0xffff, 0xff00,
			      0, 0, 0, 0, 0).GetAddress());
}

TEST(IPv6AddressTest, And)
{
	EXPECT_EQ((IPv6Address::MaskFromPrefix(128) &
		   IPv6Address::MaskFromPrefix(56)).GetAddress(),
		  IPv6Address::MaskFromPrefix(56).GetAddress());
	EXPECT_EQ((IPv6Address::MaskFromPrefix(48) &
		   IPv6Address(0x2a00, 0x1450, 0x4001, 0x816,
			       0, 0, 0, 0x200e, 0)).GetAddress(),
		  IPv6Address(0x2a00, 0x1450, 0x4001, 0,
			      0, 0, 0, 0, 0).GetAddress());
	EXPECT_EQ((IPv6Address::MaskFromPrefix(24) &
		   IPv6Address(0x2a00, 0x1450, 0x4001, 0x816,
			       0, 0, 0, 0x200e, 0)).GetAddress(),
		  IPv6Address(0x2a00, 0x1400, 0, 0,
			      0, 0, 0, 0, 0).GetAddress());
}
