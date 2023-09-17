// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "net/IPv6Address.hxx"
#include "util/Compiler.h"

#include <gtest/gtest.h>

#if GCC_CHECK_VERSION(11,0)
/* suppress warning for calling GetSize() on uninitialized object */
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

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

TEST(IPv6Address, Port)
{
	EXPECT_EQ(IPv6Address(0).GetPort(), 0);
	EXPECT_EQ(IPv6Address(1).GetPort(), 1);
	EXPECT_EQ(IPv6Address(1234).GetPort(), 1234);
	EXPECT_EQ(IPv6Address(0xffff).GetPort(), 0xffff);
}
