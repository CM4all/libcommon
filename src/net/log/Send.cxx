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

#include "Send.hxx"
#include "Datagram.hxx"
#include "Protocol.hxx"
#include "Crc.hxx"
#include "net/SocketDescriptor.hxx"
#include "net/SendMessage.hxx"
#include "io/Iovec.hxx"
#include "util/ByteOrder.hxx"
#include "util/StaticArray.hxx"

#include <sys/socket.h>

static struct iovec
MakeIovec(const char *s) noexcept
{
	return { const_cast<char *>(s), strlen(s) + 1 };
}

static constexpr struct iovec
MakeIovec(std::string_view s) noexcept
{
	return MakeIovec(std::span{s});
}

namespace Net {
namespace Log {

template<Attribute value>
static auto
MakeIovecAttribute()
{
	return MakeIovecStatic<Attribute, value>();
}

void
Send(SocketDescriptor s, const Datagram &d)
{
	StaticArray<struct iovec, 64> v;

	v.push_back(MakeIovecStatic<uint32_t, ToBE32(MAGIC_V2)>());

	uint64_t timestamp;

	if (d.HasTimestamp()) {
		v.push_back(MakeIovecAttribute<Attribute::TIMESTAMP>());
		timestamp = ToBE64(d.timestamp.time_since_epoch().count());
		v.push_back(MakeIovecT(timestamp));
	}

	if (d.remote_host != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::REMOTE_HOST>());
		v.push_back(MakeIovec(d.remote_host));
	}

	if (d.host != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::HOST>());
		v.push_back(MakeIovec(d.host));
	}

	if (d.site != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::SITE>());
		v.push_back(MakeIovec(d.site));
	}

	if (d.forwarded_to != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::FORWARDED_TO>());
		v.push_back(MakeIovec(d.forwarded_to));
	}

	uint8_t http_method;
	if (d.HasHttpMethod()) {
		v.push_back(MakeIovecAttribute<Attribute::HTTP_METHOD>());
		http_method = uint8_t(d.http_method);
		v.push_back(MakeIovecT(http_method));
	}

	if (d.http_uri != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::HTTP_URI>());
		v.push_back(MakeIovec(d.http_uri));
	}

	if (d.http_referer != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::HTTP_REFERER>());
		v.push_back(MakeIovec(d.http_referer));
	}

	if (d.user_agent != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::USER_AGENT>());
		v.push_back(MakeIovec(d.user_agent));
	}

	if (d.message != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::MESSAGE>());
		v.push_back(MakeIovec(d.message));
		v.push_back(MakeIovecStatic<uint8_t, 0>());
	}

	uint16_t http_status;
	if (d.HasHttpStatus()) {
		v.push_back(MakeIovecAttribute<Attribute::HTTP_STATUS>());
		http_status = ToBE16(int(d.http_status));
		v.push_back(MakeIovecT(http_status));
	}

	uint64_t length;
	if (d.valid_length) {
		v.push_back(MakeIovecAttribute<Attribute::LENGTH>());
		length = ToBE64(d.length);
		v.push_back(MakeIovecT(length));
	}

	struct {
		uint64_t received, sent;
	} traffic;

	if (d.valid_traffic) {
		v.push_back(MakeIovecAttribute<Attribute::TRAFFIC>());
		traffic.received = ToBE64(d.traffic_received);
		traffic.sent = ToBE64(d.traffic_sent);
		v.push_back(MakeIovecT(traffic));
	}

	uint64_t duration;
	if (d.valid_duration) {
		v.push_back(MakeIovecAttribute<Attribute::DURATION>());
		duration = ToBE64(d.duration.count());
		v.push_back(MakeIovecT(duration));
	}

	if (d.type != Net::Log::Type::UNSPECIFIED) {
		v.push_back(MakeIovecAttribute<Attribute::TYPE>());
		v.push_back(MakeIovecT(d.type));
	}

	Crc crc;

	const auto begin = std::next(v.begin()), end = v.end();
	for (auto i = begin; i != end; ++i)
		crc.Update({i->iov_base, i->iov_len});

	const uint32_t crc_value = ToBE32(crc.Finish());
	v.push_back(MakeIovecT(crc_value));

	SendMessage(s, std::span{v}, MSG_DONTWAIT);
}

}}
