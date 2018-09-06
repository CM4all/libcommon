/*
 * Copyright 2007-2017 Content Management AG
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

#include "Parser.hxx"
#include "Datagram.hxx"
#include "Protocol.hxx"
#include "Crc.hxx"
#include "util/ByteOrder.hxx"

#include <assert.h>
#include <string.h>

namespace Net {
namespace Log {

class Deserializer {
	const uint8_t *p;
	const uint8_t *end;

public:
	Deserializer(ConstBuffer<void> src) noexcept
		:p((const uint8_t *)src.data), end(p + src.size) {
		assert(p != nullptr);
		assert(end != nullptr);
		assert(p <= end);
	}

	bool empty() const noexcept {
		return p == end;
	}

	uint8_t ReadByte() {
		return *(const uint8_t *)ReadRaw(1);
	}

	uint16_t ReadU16() {
		return FromBE16(ReadT<uint16_t>());
	}

	uint64_t ReadU64() {
		return FromBE64(ReadT<uint64_t>());
	}

	StringView ReadStringView() {
		const char *result = (const char *)p;
		/* buffer is null-terminated, so strlen() is legal */
		const size_t length = strlen(result);
		const uint8_t *after = (const uint8_t *)(result + length + 1);
		if (after > end)
			throw ProtocolError();
		p = after;
		return {result, length};
	}

	const char *ReadString() {
		return ReadStringView().data;
	}

private:
	const void *ReadRaw(size_t size) {
		const uint8_t *result = p;
		if (result + size > end)
			throw ProtocolError();

		p += size;
		return result;
	}

	template<typename T>
	T ReadT() {
		T value;
		memcpy(&value, ReadRaw(sizeof(value)), sizeof(value));
		return value;
	}
};

static void
FixUp(Datagram &d)
{
	if (d.type == Type::UNSPECIFIED && d.http_uri != nullptr)
		/* old clients don't send a type; attempt to guess the
		   type */
		d.type = d.message.IsNull()
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
			datagram.http_method = http_method_t(d.ReadByte());
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
			datagram.http_status = http_status_t(d.ReadU16());
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
		}
	}

	FixUp(datagram);
	return datagram;
}

Datagram
ParseDatagram(ConstBuffer<void> _d)
{
	auto d = ConstBuffer<uint8_t>::FromVoid(_d);

	auto magic = (const uint32_t *)(const void *)d.data;
	if (d.size < sizeof(*magic))
		throw ProtocolError();

	d.MoveFront((const uint8_t *)(magic + 1));

	if (*magic == ToBE32(MAGIC_V2)) {
		if (d.size < sizeof(Crc::value_type))
			throw ProtocolError();

		auto &expected_crc =
			*(const Crc::value_type *)(const void *)
			(d.data + d.size - sizeof(Crc::value_type));
		d.SetEnd((const uint8_t *)&expected_crc);

		Crc crc;
		crc.reset();
		crc.process_bytes(d.data, d.size);
		if (crc.checksum() != FromBE32(expected_crc))
			throw ProtocolError();

		return log_server_apply_attributes(ConstBuffer<void>(d.data, d.size));
	}

	/* allow both little-endian and big-endian magic in the V1
	   protocol due to a client bug which always used host byte
	   order for the magic */
	if (*magic != ToLE32(MAGIC_V1) && *magic != ToBE32(MAGIC_V1))
		throw ProtocolError();

	return log_server_apply_attributes(ConstBuffer<void>(d.data, d.size));
}

Datagram
ParseDatagram(const void *p, const void *end)
{
	return ParseDatagram(ConstBuffer<uint8_t>((const uint8_t *)p,
						  (const uint8_t *)end).ToVoid());
}

}} // namespace Net::Log
