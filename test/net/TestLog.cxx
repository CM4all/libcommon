// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "net/log/Send.hxx"
#include "net/log/Serializer.hxx"
#include "net/log/Parser.hxx"
#include "net/log/Datagram.hxx"
#include "net/SocketError.hxx"
#include "net/SocketPair.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "http/Method.hxx"
#include "http/Status.hxx"

#include <gtest/gtest.h>

#include <array>

#include <string.h>
#include <sys/socket.h>

static bool
StringAttributeEquals(const char *a, const char *b) noexcept
{
	return a == nullptr
		? b == nullptr
		: strcmp(a, b) == 0;
}

static bool
StringAttributeEquals(std::string_view a, std::string_view b) noexcept
{
	return a.data() == nullptr
		? b.data() == nullptr
		: a == b;
}

static bool
operator==(const Net::Log::Datagram &a, const Net::Log::Datagram &b) noexcept
{
	return a.timestamp == b.timestamp &&
		StringAttributeEquals(a.remote_host, b.remote_host) &&
		StringAttributeEquals(a.host, b.host) &&
		StringAttributeEquals(a.site, b.site) &&
		StringAttributeEquals(a.forwarded_to, b.forwarded_to) &&
		StringAttributeEquals(a.http_uri, b.http_uri) &&
		StringAttributeEquals(a.http_referer, b.http_referer) &&
		StringAttributeEquals(a.user_agent, b.user_agent) &&
		StringAttributeEquals(a.http_uri, b.http_uri) &&
		StringAttributeEquals(a.message, b.message) &&
		a.valid_length == b.valid_length &&
		(!a.valid_length || a.length == b.length) &&
		a.valid_traffic == b.valid_traffic &&
		(!a.valid_traffic ||
		 (a.traffic_received == b.traffic_received &&
		  a.traffic_sent == b.traffic_sent)) &&
		a.valid_duration == b.valid_duration &&
		(!a.valid_duration || a.duration == b.duration) &&
		a.http_method == b.http_method &&
		a.http_status == b.http_status &&
		a.type == b.type;
}

TEST(Log, Serializer)
{
	std::array<std::byte, 4096> buffer;
	Net::Log::Datagram d;

	std::fill(buffer.begin(), buffer.end(), std::byte{0xff});
	size_t size = Net::Log::Serialize(buffer, d);
	ASSERT_EQ(size, 8u);
	EXPECT_EQ(memcmp(buffer.data(), "\x63\x04\x61\x03", 4), 0);
	EXPECT_TRUE(Net::Log::ParseDatagram(std::span{buffer}.first(size)) == d);

	d.message = "foo";
	std::fill(buffer.begin(), buffer.end(), std::byte{0xff});
	size = Net::Log::Serialize(buffer, d);
	ASSERT_EQ(size, 13u);
	EXPECT_EQ(memcmp(buffer.data(), "\x63\x04\x61\x03" "\x0d" "foo\0", 9), 0);
	EXPECT_TRUE(Net::Log::ParseDatagram(std::span{buffer}.first(size)) == d);

	d.remote_host = "a";
	d.host = "b";
	d.site = "c";
	d.http_uri = "d";
	d.http_referer = "e";
	d.user_agent = "f";
	d.http_method = HttpMethod::POST;
	d.http_status = HttpStatus::NO_CONTENT;
	d.type = Net::Log::Type::SSH;
	std::fill(buffer.begin(), buffer.end(), std::byte{0xff});
	size = Net::Log::Serialize(buffer, d);
	EXPECT_TRUE(Net::Log::ParseDatagram(std::span{buffer}.first(size)) == d);

	d.timestamp = Net::Log::FromSystem(std::chrono::system_clock::now());
	d.valid_length = true;
	d.length = 0x123456789abcdef;
	d.valid_traffic = true;
	d.traffic_received = 1;
	d.traffic_sent = 2;
	d.valid_duration = true;
	d.duration = Net::Log::Duration(3);
	std::fill(buffer.begin(), buffer.end(), std::byte{0xff});
	size = Net::Log::Serialize(buffer, d);
	EXPECT_TRUE(Net::Log::ParseDatagram(std::span{buffer}.first(size)) == d);
}

static auto
SendReceive(const Net::Log::Datagram &src)
{
	auto [a, b] = CreateSocketPair(SOCK_SEQPACKET);

	Net::Log::Send(a, src);

	static std::array<std::byte, 4096> buffer;
	auto nbytes = b.Receive(buffer);
	if (nbytes < 0)
		throw MakeSocketError("Failed to receive");

	return Net::Log::ParseDatagram(std::span{buffer}.first(nbytes));
}

TEST(Log, Send)
{
	Net::Log::Datagram d;

	EXPECT_TRUE(SendReceive(d) == d);

	d.message = "foo";
	EXPECT_TRUE(SendReceive(d) == d);

	d.remote_host = "a";
	d.host = "b";
	d.site = "c";
	d.http_uri = "d";
	d.http_referer = "e";
	d.user_agent = "f";
	d.http_method = HttpMethod::POST;
	d.http_status = HttpStatus::NO_CONTENT;
	d.type = Net::Log::Type::SSH;
	EXPECT_TRUE(SendReceive(d) == d);

	d.timestamp = Net::Log::FromSystem(std::chrono::system_clock::now());
	d.valid_length = true;
	d.length = 0x123456789abcdef;
	d.valid_traffic = true;
	d.traffic_received = 1;
	d.traffic_sent = 2;
	d.valid_duration = true;
	d.duration = Net::Log::Duration(3);
	EXPECT_TRUE(SendReceive(d) == d);
}
