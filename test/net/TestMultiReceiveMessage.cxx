// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "net/MultiReceiveMessage.hxx"
#include "net/IPv4Address.hxx"
#include "net/SocketDescriptor.hxx"
#include "net/FormatAddress.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using std::string_view_literals::operator""sv;

static std::string
FormatAddress(SocketAddress address) noexcept
{
	char buffer[256];
	return ToString(buffer, address, "?");
}

static UniqueSocketDescriptor
MakeBoundUdpSocket()
{
	UniqueSocketDescriptor s;
	EXPECT_TRUE(s.CreateNonBlock(AF_INET, SOCK_DGRAM, 0));
	EXPECT_TRUE(s.Bind(IPv4Address{127, 0, 0, 1, 0}));
	return s;
}

static void
SendTo(SocketDescriptor s, SocketAddress address, std::string_view payload)
{
	ASSERT_TRUE(s.Connect(address));
	ASSERT_EQ(s.Send(std::as_bytes(std::span{payload})),
		  ssize_t(payload.size()));
}

/**
 * Each datagram in a recvmmsg() batch must be attributed to its own
 * sender, not to the sender of the last datagram in the batch.
 */
TEST(MultiReceiveMessage, PerDatagramAddress)
{
	auto r = MakeBoundUdpSocket();
	const auto r_address = r.GetLocalAddress();

	auto s1 = MakeBoundUdpSocket();
	auto s2 = MakeBoundUdpSocket();

	const auto a1 = FormatAddress(s1.GetLocalAddress());
	const auto a2 = FormatAddress(s2.GetLocalAddress());
	ASSERT_NE(a1, a2);

	/* queue both datagrams before receiving, so that recvmmsg()
	   returns them in one batch */
	SendTo(s1, r_address, "one"sv);
	SendTo(s2, r_address, "two"sv);

	MultiReceiveMessage multi{16, 1024};
	ASSERT_TRUE(multi.Receive(r));

	std::vector<std::pair<std::string, std::string>> received;
	for (const auto &d : multi)
		received.emplace_back(std::string{(const char *)d.payload.data(),
						  d.payload.size()},
				      FormatAddress(d.address));

	ASSERT_EQ(received.size(), 2U);
	EXPECT_EQ(received[0].first, "one");
	EXPECT_EQ(received[1].first, "two");
	EXPECT_EQ(received[0].second, a1);
	EXPECT_EQ(received[1].second, a2);
}
