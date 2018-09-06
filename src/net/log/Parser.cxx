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

#include <string.h>

using namespace Net::Log;

template<typename T>
static const void *
ReadT(T &value_r, const void *p, const uint8_t *end)
{
	if ((const uint8_t *)p + sizeof(value_r) > end)
		throw ProtocolError();

	auto src = (const T *)p;
	memcpy(&value_r, src, sizeof(value_r));
	return src + 1;

}

static const void *
read_uint8(uint8_t *value_r, const void *p, const uint8_t *end)
{
	return ReadT<uint8_t>(*value_r, p, end);
}

static const void *
read_uint16(uint16_t *value_r, const void *p, const uint8_t *end)
{
	uint16_t value;
	p = ReadT<uint16_t>(value, p, end);
	*value_r = FromBE16(value);
	return p;
}

static const void *
read_uint64(uint64_t *value_r, const void *p, const uint8_t *end)
{
	uint64_t value;
	p = ReadT<uint64_t>(value, p, end);
	*value_r = FromBE64(value);
	return p;
}

static const void *
read_string(const char **value_r, const void *p, const uint8_t *end)
{
	auto q = (const char *)p;

	*value_r = q;

	q += strlen(q) + 1;
	if (q > (const char *)end)
		throw ProtocolError();

	return q;
}

static const void *
ReadStringView(StringView &value_r, const void *p, const uint8_t *end)
{
	const char *value;
	p = read_string(&value, p, end);
	value_r = value;
	return p;
}

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
log_server_apply_attributes(const void *p, const uint8_t *end)
{
	assert(p != nullptr);
	assert(end != nullptr);
	assert((const char *)p < (const char *)end);

	Datagram datagram;

	while (true) {
		auto attr_p = (const uint8_t *)p;
		if (attr_p >= end) {
			FixUp(datagram);
			return datagram;
		}

		auto attr = (Attribute)*attr_p++;
		p = attr_p;

		switch (attr) {
			uint8_t u8;
			uint16_t u16;

		case Attribute::NOP:
			break;

		case Attribute::TIMESTAMP:
			p = read_uint64(&datagram.timestamp, p, end);
			datagram.valid_timestamp = true;
			break;

		case Attribute::REMOTE_HOST:
			p = read_string(&datagram.remote_host, p, end);
			break;

		case Attribute::FORWARDED_TO:
			p = read_string(&datagram.forwarded_to, p, end);
			break;

		case Attribute::HOST:
			p = read_string(&datagram.host, p, end);
			break;

		case Attribute::SITE:
			p = read_string(&datagram.site, p, end);
			break;

		case Attribute::HTTP_METHOD:
			p = read_uint8(&u8, p, end);
			if (p == nullptr)
				throw ProtocolError();

			datagram.http_method = http_method_t(u8);
			if (!http_method_is_valid(datagram.http_method))
				throw ProtocolError();

			datagram.valid_http_method = true;
			break;

		case Attribute::HTTP_URI:
			p = read_string(&datagram.http_uri, p, end);
			break;

		case Attribute::HTTP_REFERER:
			p = read_string(&datagram.http_referer, p, end);
			break;

		case Attribute::USER_AGENT:
			p = read_string(&datagram.user_agent, p, end);
			break;

		case Attribute::MESSAGE:
			p = ReadStringView(datagram.message, p, end);
			break;

		case Attribute::HTTP_STATUS:
			p = read_uint16(&u16, p, end);
			if (p == nullptr)
				throw ProtocolError();

			datagram.http_status = http_status_t(u16);
			if (!http_status_is_valid(datagram.http_status))
				throw ProtocolError();

			datagram.valid_http_status = true;
			break;

		case Attribute::LENGTH:
			p = read_uint64(&datagram.length, p, end);
			datagram.valid_length = true;
			break;

		case Attribute::TRAFFIC:
			p = read_uint64(&datagram.traffic_received, p, end);
			p = read_uint64(&datagram.traffic_sent, p, end);
			datagram.valid_traffic = true;
			break;

		case Attribute::DURATION:
			p = read_uint64(&datagram.duration, p, end);
			datagram.valid_duration = true;
			break;

		case Attribute::TYPE:
			p = read_uint8(&(uint8_t &)datagram.type, p, end);
			break;
		}
	}
}

Datagram
Net::Log::ParseDatagram(ConstBuffer<void> _d)
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

		return log_server_apply_attributes(d.data, d.data + d.size);
	}

	/* allow both little-endian and big-endian magic in the V1
	   protocol due to a client bug which always used host byte
	   order for the magic */
	if (*magic != ToLE32(MAGIC_V1) && *magic != ToBE32(MAGIC_V1))
		throw ProtocolError();

	return log_server_apply_attributes(d.data, d.data + d.size);
}

Datagram
Net::Log::ParseDatagram(const void *p, const void *end)
{
	return ParseDatagram(ConstBuffer<uint8_t>((const uint8_t *)p,
						  (const uint8_t *)end).ToVoid());
}
