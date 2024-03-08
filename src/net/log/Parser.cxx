// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Parser.hxx"
#include "Datagram.hxx"
#include "Protocol.hxx"
#include "Crc.hxx"
#include "http/Method.hxx"
#include "http/Status.hxx"
#include "util/PackedBigEndian.hxx"

#include <assert.h>
#include <string.h>

namespace Net::Log {

class Deserializer {
	const std::byte *p;
	const std::byte *end;

public:
	Deserializer(std::span<const std::byte> src) noexcept
		:p(src.data()), end(p + src.size()) {
		assert(p != nullptr);
		assert(end != nullptr);
		assert(p <= end);
	}

	bool empty() const noexcept {
		return p == end;
	}

	std::byte ReadByte() {
		return ReadT<std::byte>();
	}

	uint16_t ReadU16() {
		return ReadT<PackedBE16>();
	}

	uint64_t ReadU64() {
		return ReadT<PackedBE64>();
	}

	std::string_view ReadStringView() {
		const char *result = (const char *)p;
		const std::size_t remaining = (const char *)end - result;
		const size_t length = strnlen(result, remaining);
		if (length >= remaining)
			/* null terminator not found */
			throw ProtocolError();

		const std::byte *after = (const std::byte *)(result + length + 1);
		p = after;
		return {result, length};
	}

	const char *ReadString() {
		return ReadStringView().data();
	}

private:
	const std::byte *ReadRaw(size_t size) {
		const std::byte *result = p;
		if (result + size > end)
			throw ProtocolError();

		p += size;
		return result;
	}

	template<typename T>
	requires(alignof(T) == 1)
	const T &ReadT() {
		return *reinterpret_cast<const T *>(ReadRaw(sizeof(T)));
	}
};

static void
FixUp(Datagram &d)
{
	if (d.type == Type::UNSPECIFIED && d.http_uri != nullptr)
		/* old clients don't send a type; attempt to guess the
		   type */
		d.type = d.message.data() == nullptr
			? Type::HTTP_ACCESS
			: Type::HTTP_ERROR;
}

static Datagram
log_server_apply_attributes(Deserializer d)
{
	Datagram datagram;

	while (!d.empty()) {
		const auto attr = Attribute(d.ReadByte());

		switch (attr) {
		case Attribute::NOP:
			break;

		case Attribute::TIMESTAMP:
			datagram.timestamp = Net::Log::TimePoint(Net::Log::Duration(d.ReadU64()));
			break;

		case Attribute::REMOTE_HOST:
			datagram.remote_host = d.ReadString();
			break;

		case Attribute::FORWARDED_TO:
			datagram.forwarded_to = d.ReadString();
			break;

		case Attribute::HOST:
			datagram.host = d.ReadString();
			break;

		case Attribute::SITE:
			datagram.site = d.ReadString();
			break;

		case Attribute::HTTP_METHOD:
			datagram.http_method = static_cast<HttpMethod>(d.ReadByte());
			if (!http_method_is_valid(datagram.http_method))
				throw ProtocolError();

			break;

		case Attribute::HTTP_URI:
			datagram.http_uri = d.ReadString();
			break;

		case Attribute::HTTP_REFERER:
			datagram.http_referer = d.ReadString();
			break;

		case Attribute::USER_AGENT:
			datagram.user_agent = d.ReadString();
			break;

		case Attribute::MESSAGE:
			datagram.message = d.ReadString();
			break;

		case Attribute::HTTP_STATUS:
			datagram.http_status = static_cast<HttpStatus>(d.ReadU16());
			if (!http_status_is_valid(datagram.http_status))
				throw ProtocolError();

			break;

		case Attribute::LENGTH:
			datagram.length = d.ReadU64();
			datagram.valid_length = true;
			break;

		case Attribute::TRAFFIC:
			datagram.traffic_received = d.ReadU64();
			datagram.traffic_sent = d.ReadU64();
			datagram.valid_traffic = true;
			break;

		case Attribute::DURATION:
			datagram.duration = Duration(d.ReadU64());
			datagram.valid_duration = true;
			break;

		case Attribute::TYPE:
			datagram.type = Type(d.ReadByte());
			break;

		case Attribute::JSON:
			datagram.json = d.ReadString();
			break;

		case Attribute::ANALYTICS_ID:
			datagram.analytics_id = d.ReadString();
			break;
		}
	}

	FixUp(datagram);
	return datagram;
}

Datagram
ParseDatagram(std::span<const std::byte> d)
{
	auto magic = (const uint32_t *)(const void *)d.data();
	if (d.size() < sizeof(*magic))
		throw ProtocolError();

	d = d.subspan(sizeof(*magic));

	if (*magic == ToBE32(MAGIC_V2)) {
		if (d.size() < sizeof(Crc::value_type))
			throw ProtocolError();

		auto &expected_crc =
			*(const Crc::value_type *)(const void *)
			(d.data() + d.size() - sizeof(Crc::value_type));
		d = d.first(d.size() - sizeof(expected_crc));

		Crc crc;
		crc.Update(d);
		if (crc.Finish() != FromBE32(expected_crc))
			throw ProtocolError();

		return log_server_apply_attributes(d);
	}

	return log_server_apply_attributes(d);
}

} // namespace Net::Log
