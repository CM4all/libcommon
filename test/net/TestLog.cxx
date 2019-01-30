/*
 * Copyright 2007-2019 Content Management AG
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

#include "net/log/Serializer.hxx"
#include "net/log/Parser.hxx"
#include "net/log/Datagram.hxx"

#include <gtest/gtest.h>

#include <string.h>

static bool
StringAttributeEquals(const char *a, const char *b) noexcept
{
	return a == nullptr
		? b == nullptr
		: strcmp(a, b) == 0;
}

static bool
StringAttributeEquals(StringView a, StringView b) noexcept
{
	return a == nullptr
		? b == nullptr
		: (a.size == b.size &&
		   memcmp(a.data, b.data, a.size) == 0);
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
	uint8_t buffer[4096];
	Net::Log::Datagram d;

	memset(buffer, 0xff, sizeof(buffer));
	size_t size = Net::Log::Serialize(buffer, sizeof(buffer), d);
	ASSERT_EQ(size, 8);
	EXPECT_EQ(memcmp(buffer, "\x63\x04\x61\x03", 4), 0);
	EXPECT_TRUE(Net::Log::ParseDatagram({buffer, size}) == d);

	d.message = "foo";
	memset(buffer, 0xff, sizeof(buffer));
	size = Net::Log::Serialize(buffer, sizeof(buffer), d);
	ASSERT_EQ(size, 13);
	EXPECT_EQ(memcmp(buffer, "\x63\x04\x61\x03" "\x0d" "foo\0", 9), 0);
	EXPECT_TRUE(Net::Log::ParseDatagram({buffer, size}) == d);

	d.remote_host = "a";
	d.host = "b";
	d.site = "c";
	d.http_uri = "d";
	d.http_referer = "e";
	d.user_agent = "f";
	d.http_method = HTTP_METHOD_POST;
	d.http_status = HTTP_STATUS_NO_CONTENT;
	d.type = Net::Log::Type::SSH;
	memset(buffer, 0xff, sizeof(buffer));
	size = Net::Log::Serialize(buffer, sizeof(buffer), d);
	EXPECT_TRUE(Net::Log::ParseDatagram({buffer, size}) == d);

	d.timestamp = Net::Log::FromSystem(std::chrono::system_clock::now());
	d.valid_length = true;
	d.length = 0x123456789abcdef;
	d.valid_traffic = true;
	d.traffic_received = 1;
	d.traffic_sent = 2;
	d.valid_duration = true;
	d.duration = Net::Log::Duration(3);
	memset(buffer, 0xff, sizeof(buffer));
	size = Net::Log::Serialize(buffer, sizeof(buffer), d);
	EXPECT_TRUE(Net::Log::ParseDatagram({buffer, size}) == d);
}
