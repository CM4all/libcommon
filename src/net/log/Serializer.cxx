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

#include "Serializer.hxx"
#include "Datagram.hxx"
#include "Protocol.hxx"
#include "Crc.hxx"
#include "util/ByteOrder.hxx"

#include <sys/socket.h>

namespace Net {
namespace Log {

namespace {

class BufferWriter {
	uint8_t *const begin, *const end;
	uint8_t *p;

public:
	BufferWriter(uint8_t *_begin, uint8_t *_end) noexcept
		:begin(_begin), end(_end), p(begin) {}

	BufferWriter(uint8_t *_begin, size_t size) noexcept
		:BufferWriter(_begin, _begin + size) {}

	BufferWriter(void *_p, size_t size) noexcept
		:BufferWriter((uint8_t *)_p, size) {}

	size_t size() const noexcept {
		return p - begin;
	}

	void *WriteN(size_t nbytes) {
		void *result = p;

		p += nbytes;
		if (p > end)
			throw BufferTooSmall();

		return result;
	}

	template<typename T>
	T *WriteT() {
		return (T *)WriteN(sizeof(T));
	}

	template<typename T>
	void WriteT(const T &value) {
		*WriteT<T>() = value;
	}

	void WriteByte(uint8_t value) {
		WriteT<uint8_t>(value);
	}

	void WriteAttribute(Attribute value) {
		WriteT(value);
	}

	void WriteBE16(uint16_t value) {
		WriteT<uint16_t>(ToBE16(value));
	}

	void WriteBE32(uint32_t value) {
		WriteT<uint32_t>(ToBE32(value));
	}

	void WriteBE64(uint64_t value) {
		WriteT<uint64_t>(ToBE64(value));
	}

	void WriteString(const char *value) {
		size_t nbytes = strlen(value) + 1;
		memcpy(WriteN(nbytes), value, nbytes);
	}

	void WriteString(StringView value) {
		void *dest = WriteN(value.size + 1);
		*(char *)mempcpy(dest, value.data, value.size) = 0;
	}

	void WriteOptionalString(Attribute a, const char *value) {
		if (value == nullptr)
			return;

		WriteAttribute(a);
		WriteString(value);
	}
};

}

size_t
Serialize(void *buffer, size_t size, const Datagram &d)
{
	BufferWriter w(buffer, size);

	w.WriteBE32(MAGIC_V2);

	if (d.HasTimestamp()) {
		w.WriteAttribute(Attribute::TIMESTAMP);
		w.WriteBE64(d.timestamp.time_since_epoch().count());
	}

	w.WriteOptionalString(Attribute::REMOTE_HOST, d.remote_host);
	w.WriteOptionalString(Attribute::HOST, d.host);
	w.WriteOptionalString(Attribute::SITE, d.site);
	w.WriteOptionalString(Attribute::FORWARDED_TO, d.forwarded_to);

	if (d.HasHttpMethod()) {
		w.WriteAttribute(Attribute::HTTP_METHOD);
		w.WriteByte((uint8_t)d.http_method);
	}

	w.WriteOptionalString(Attribute::HTTP_URI, d.http_uri);
	w.WriteOptionalString(Attribute::HTTP_REFERER, d.http_referer);
	w.WriteOptionalString(Attribute::USER_AGENT, d.user_agent);

	if (d.message != nullptr) {
		w.WriteAttribute(Attribute::MESSAGE);
		w.WriteString(d.message);
	}

	if (d.HasHttpStatus()) {
		w.WriteAttribute(Attribute::HTTP_STATUS);
		w.WriteBE16(uint16_t(d.http_status));
	}

	if (d.valid_length) {
		w.WriteAttribute(Attribute::LENGTH);
		w.WriteBE64(d.length);
	}

	if (d.valid_traffic) {
		w.WriteAttribute(Attribute::TRAFFIC);
		w.WriteBE64(d.traffic_received);
		w.WriteBE64(d.traffic_sent);
	}

	if (d.valid_duration) {
		w.WriteAttribute(Attribute::DURATION);
		w.WriteBE64(d.duration.count());
	}

	if (d.type != Net::Log::Type::UNSPECIFIED) {
		w.WriteAttribute(Attribute::TYPE);
		w.WriteT(d.type);
	}

	Crc crc;
	crc.reset();
	crc.process_bytes((const uint8_t *)buffer + 4, w.size() - 4);

	w.WriteBE32(crc.checksum());

	return w.size();
}

}}
