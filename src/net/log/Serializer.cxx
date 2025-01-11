// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Serializer.hxx"
#include "Datagram.hxx"
#include "Protocol.hxx"
#include "Crc.hxx"
#include "util/PackedBigEndian.hxx"

#include <sys/socket.h>
#include <string.h>

namespace Net::Log {

namespace {

class BufferWriter {
	std::byte *const begin, *const end;
	std::byte *p;

public:
	BufferWriter(std::byte *_begin, std::byte *_end) noexcept
		:begin(_begin), end(_end), p(begin) {}

	BufferWriter(std::byte *_begin, std::size_t size) noexcept
		:BufferWriter(_begin, _begin + size) {}

	BufferWriter(void *_p, std::size_t size) noexcept
		:BufferWriter((std::byte *)_p, size) {}

	std::size_t size() const noexcept {
		return p - begin;
	}

	void *WriteN(std::size_t nbytes) {
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

	void WriteByte(std::byte value) {
		WriteT<std::byte>(value);
	}

	void WriteAttribute(Attribute value) {
		WriteT(value);
	}

	void WriteBE16(uint16_t value) {
		WriteT(PackedBE16{value});
	}

	void WriteBE32(uint32_t value) {
		WriteT(PackedBE32{value});
	}

	void WriteBE64(uint64_t value) {
		WriteT(PackedBE64{value});
	}

	void WriteString(const char *value) {
		std::size_t nbytes = strlen(value) + 1;
		memcpy(WriteN(nbytes), value, nbytes);
	}

	void WriteString(std::string_view value) {
		void *dest = WriteN(value.size() + 1);
		*(char *)mempcpy(dest, value.data(), value.size()) = 0;
	}

	void WriteOptionalString(Attribute a, const char *value) {
		if (value == nullptr)
			return;

		WriteAttribute(a);
		WriteString(value);
	}
};

}

std::size_t
Serialize(void *buffer, std::size_t size, const Datagram &d)
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
		w.WriteByte((std::byte)d.http_method);
	}

	w.WriteOptionalString(Attribute::HTTP_URI, d.http_uri);
	w.WriteOptionalString(Attribute::HTTP_REFERER, d.http_referer);
	w.WriteOptionalString(Attribute::USER_AGENT, d.user_agent);

	if (d.message.data() != nullptr) {
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

	if (d.type != Type::UNSPECIFIED) {
		w.WriteAttribute(Attribute::TYPE);
		w.WriteT(d.type);
	}

	w.WriteOptionalString(Attribute::ANALYTICS_ID, d.analytics_id);
	w.WriteOptionalString(Attribute::GENERATOR, d.generator);

	if (d.content_type != ContentType{}) {
		w.WriteAttribute(Attribute::CONTENT_TYPE);
		w.WriteT(d.content_type);
	}

	Crc crc;
	crc.Update({(const std::byte *)buffer + 4, w.size() - 4});

	w.WriteBE32(crc.Finish());

	return w.size();
}

} // namespace Net::Log
