// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Send.hxx"
#include "Datagram.hxx"
#include "Protocol.hxx"
#include "Crc.hxx"
#include "net/SocketDescriptor.hxx"
#include "net/SendMessage.hxx"
#include "io/Iovec.hxx"
#include "util/ByteOrder.hxx"
#include "util/SpanCast.hxx"
#include "util/StaticVector.hxx"

#include <string.h>
#include <sys/socket.h>

static void
PushString(auto &v, const char *s) noexcept
{
	v.emplace_back(MakeIovec(std::span{const_cast<char *>(s), strlen(s) + 1}));
}

static void
PushString(auto &v, std::string_view s) noexcept
{
	v.push_back(MakeIovec(AsBytes(s)));
	v.push_back(MakeIovecStatic<char, 0>());
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
	StaticVector<struct iovec, 64> v;

	v.push_back(MakeIovecStatic<uint32_t, ToBE32(MAGIC_V2)>());

	struct {
		Attribute attribute;
		PackedBE64 value;
	} timestamp;

	if (d.HasTimestamp()) {
		timestamp.attribute = Attribute::TIMESTAMP;
		timestamp.value = d.timestamp.time_since_epoch().count();
		v.push_back(MakeIovecT(timestamp));
	}

	if (d.remote_host != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::REMOTE_HOST>());
		PushString(v, d.remote_host);
	}

	if (d.host != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::HOST>());
		PushString(v, d.host);
	}

	if (d.site != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::SITE>());
		PushString(v, d.site);
	}

	if (d.forwarded_to != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::FORWARDED_TO>());
		PushString(v, d.forwarded_to);
	}

	struct {
		Attribute attribute;
		uint8_t value;
	} http_method;

	if (d.HasHttpMethod()) {
		http_method.attribute = Attribute::HTTP_METHOD;
		http_method.value = uint8_t(d.http_method);
		v.push_back(MakeIovecT(http_method));
	}

	if (d.http_uri != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::HTTP_URI>());
		PushString(v, d.http_uri);
	}

	if (d.http_referer != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::HTTP_REFERER>());
		PushString(v, d.http_referer);
	}

	if (d.user_agent != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::USER_AGENT>());
		PushString(v, d.user_agent);
	}

	if (d.message.data() != nullptr) {
		v.push_back(MakeIovecAttribute<Attribute::MESSAGE>());
		PushString(v, d.message);
	}

	struct {
		Attribute attribute;
		PackedBE16 value;
	} http_status;

	if (d.HasHttpStatus()) {
		http_status.attribute = Attribute::HTTP_STATUS;
		http_status.value = unsigned(d.http_status);
		v.push_back(MakeIovecT(http_status));
	}

	struct {
		Attribute attribute;
		PackedBE64 value;
	} length;

	if (d.valid_length) {
		length.attribute = Attribute::LENGTH;
		length.value = d.length;
		v.push_back(MakeIovecT(length));
	}

	struct {
		Attribute attribute;
		PackedBE64 received, sent;
	} traffic;

	if (d.valid_traffic) {
		traffic.attribute = Attribute::TRAFFIC;
		traffic.received = d.traffic_received;
		traffic.sent = d.traffic_sent;
		v.push_back(MakeIovecT(traffic));
	}

	struct {
		Attribute attribute;
		PackedBE64 value;
	} duration;

	if (d.valid_duration) {
		duration.attribute = Attribute::DURATION;
		duration.value = d.duration.count();
		v.push_back(MakeIovecT(duration));
	}

	struct {
		Attribute attribute;
		Net::Log::Type value;
	} type;

	if (d.type != Net::Log::Type::UNSPECIFIED) {
		type.attribute = Attribute::TYPE;
		type.value = d.type;
		v.push_back(MakeIovecT(type));
	}

	Crc crc;

	const auto begin = std::next(v.begin()), end = v.end();
	for (auto i = begin; i != end; ++i)
		crc.Update(ToSpan(*i));

	const uint32_t crc_value = ToBE32(crc.Finish());
	v.push_back(MakeIovecT(crc_value));

	SendMessage(s, std::span{v}, MSG_DONTWAIT);
}

}}
