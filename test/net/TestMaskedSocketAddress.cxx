// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "net/MaskedSocketAddress.hxx"
#include "net/Literals.hxx"
#include "net/Parser.hxx"

#include <gtest/gtest.h>

TEST(MaskedSocketAddressTest, Local)
{
	const MaskedSocketAddress a("@foo");
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_TRUE(a.Matches(ParseSocketAddress("@foo", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("/run/foo", 42, false)));

	const MaskedSocketAddress b("/run/foo");
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("@foo", 42, false)));
	EXPECT_TRUE(b.Matches(ParseSocketAddress("/run/foo", 42, false)));
}

TEST(MaskedSocketAddressTest, IPv4)
{
	const MaskedSocketAddress a("192.168.1.2");
	EXPECT_TRUE(a.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"10.0.0.1"_ipv4}));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("@foo", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("/run/foo", 42, false)));

	const MaskedSocketAddress b("192.168.1.0/24");
	EXPECT_TRUE(b.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_TRUE(b.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(b.Matches(SocketAddress{"10.0.0.1"_ipv4}));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("@foo", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("/run/foo", 42, false)));

	const MaskedSocketAddress c("0.0.0.0");
	EXPECT_TRUE(c.Matches(SocketAddress{"0.0.0.0"_ipv4}));
	EXPECT_FALSE(c.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(c.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(c.Matches(SocketAddress{"10.0.0.1"_ipv4}));
	EXPECT_FALSE(c.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(c.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_FALSE(c.Matches(ParseSocketAddress("@foo", 42, false)));
	EXPECT_FALSE(c.Matches(ParseSocketAddress("/run/foo", 42, false)));

	const MaskedSocketAddress d("0.0.0.0/0");
	EXPECT_TRUE(d.Matches(SocketAddress{"0.0.0.0"_ipv4}));
	EXPECT_TRUE(d.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_TRUE(d.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_TRUE(d.Matches(SocketAddress{"10.0.0.1"_ipv4}));
	EXPECT_FALSE(d.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(d.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_FALSE(d.Matches(ParseSocketAddress("@foo", 42, false)));
	EXPECT_FALSE(d.Matches(ParseSocketAddress("/run/foo", 42, false)));

	EXPECT_THROW(MaskedSocketAddress("192.168.1.0/16"), std::runtime_error);
	EXPECT_THROW(MaskedSocketAddress("192.168.1.0/33"), std::runtime_error);
}

TEST(MaskedSocketAddressTest, IPv6)
{
	const MaskedSocketAddress a("1234:5678:90ab::cdef");
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_TRUE(a.Matches(ParseSocketAddress("1234:5678:90ab::cdef", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("@foo", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("/run/foo", 42, false)));

	const MaskedSocketAddress b("1234:5678::/32");
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_TRUE(b.Matches(ParseSocketAddress("1234:5678:90ab::cdef", 42, false)));
	EXPECT_TRUE(b.Matches(ParseSocketAddress("1234:5678:90ab::1", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("@foo", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("/run/foo", 42, false)));

	EXPECT_THROW(MaskedSocketAddress("1234:5678::/16"), std::runtime_error);
	EXPECT_THROW(MaskedSocketAddress("1234:5678::/129"), std::runtime_error);
}
