// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "net/MaskedSocketAddress.hxx"
#include "net/Literals.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/Parser.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(MaskedSocketAddressTest, Local)
{
	const MaskedSocketAddress a("@foo");
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.3"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::1", 42, false)));
#endif
	EXPECT_TRUE(a.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(a.Matches(LocalSocketAddress{"/run/foo"sv}));

	const MaskedSocketAddress b("/run/foo");
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.3"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::1", 42, false)));
#endif
	EXPECT_FALSE(b.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_TRUE(b.Matches(LocalSocketAddress{"/run/foo"sv}));
}

TEST(MaskedSocketAddressTest, IPv4)
{
	const MaskedSocketAddress a("192.168.1.2");
	EXPECT_TRUE(a.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"10.0.0.1"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::1", 42, false)));
#endif
	EXPECT_FALSE(a.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(a.Matches(LocalSocketAddress{"/run/foo"sv}));

	const MaskedSocketAddress b("192.168.1.0/24");
	EXPECT_TRUE(b.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_TRUE(b.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(b.Matches(SocketAddress{"10.0.0.1"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::1", 42, false)));
#endif
	EXPECT_FALSE(b.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(b.Matches(LocalSocketAddress{"/run/foo"sv}));

	const MaskedSocketAddress c("0.0.0.0");
	EXPECT_TRUE(c.Matches(SocketAddress{"0.0.0.0"_ipv4}));
	EXPECT_FALSE(c.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(c.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(c.Matches(SocketAddress{"10.0.0.1"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(c.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(c.Matches(ParseSocketAddress("::1", 42, false)));
#endif
	EXPECT_FALSE(c.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(c.Matches(LocalSocketAddress{"/run/foo"sv}));

	const MaskedSocketAddress d("0.0.0.0/0");
	EXPECT_TRUE(d.Matches(SocketAddress{"0.0.0.0"_ipv4}));
	EXPECT_TRUE(d.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_TRUE(d.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_TRUE(d.Matches(SocketAddress{"10.0.0.1"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(d.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(d.Matches(ParseSocketAddress("::1", 42, false)));
#endif
	EXPECT_FALSE(d.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(d.Matches(LocalSocketAddress{"/run/foo"sv}));

	EXPECT_THROW(MaskedSocketAddress("192.168.1.0/16"), std::runtime_error);
	EXPECT_THROW(MaskedSocketAddress("192.168.1.0/33"), std::runtime_error);
}

#ifdef HAVE_IPV6

TEST(MaskedSocketAddressTest, IPv6)
{
	const MaskedSocketAddress a("1234:5678:90ab::cdef");
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(a.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_TRUE(a.Matches(ParseSocketAddress("1234:5678:90ab::cdef", 42, false)));
	EXPECT_FALSE(a.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(a.Matches(LocalSocketAddress{"/run/foo"sv}));

	const MaskedSocketAddress b("1234:5678::/32");
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::", 42, false)));
	EXPECT_FALSE(b.Matches(ParseSocketAddress("::1", 42, false)));
	EXPECT_TRUE(b.Matches(ParseSocketAddress("1234:5678:90ab::cdef", 42, false)));
	EXPECT_TRUE(b.Matches(ParseSocketAddress("1234:5678:90ab::1", 42, false)));
	EXPECT_FALSE(b.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(b.Matches(LocalSocketAddress{"/run/foo"sv}));

	EXPECT_THROW(MaskedSocketAddress("1234:5678::/16"), std::runtime_error);
	EXPECT_THROW(MaskedSocketAddress("1234:5678::/129"), std::runtime_error);
}

#endif // HAVE_IPV6
