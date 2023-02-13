// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef BENG_PROXY_SPAWN_BUILDER_HXX
#define BENG_PROXY_SPAWN_BUILDER_HXX

#include "net/SocketDescriptor.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "net/SendMessage.hxx"
#include "IProtocol.hxx"
#include "system/Error.hxx"
#include "io/FileDescriptor.hxx"
#include "io/Iovec.hxx"
#include "util/SpanCast.hxx"
#include "util/StaticVector.hxx"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include <assert.h>
#include <stdint.h>
#include <string.h>

class SpawnPayloadTooLargeError {};

class SpawnSerializer {
	size_t size = 0;

	std::array<std::byte, 65536> buffer;

	StaticVector<FileDescriptor, 8> fds;

public:
	explicit SpawnSerializer(SpawnRequestCommand cmd) {
		buffer[size++] = static_cast<std::byte>(cmd);
	}

	explicit SpawnSerializer(SpawnResponseCommand cmd) {
		buffer[size++] = static_cast<std::byte>(cmd);
	}

	void WriteByte(std::byte value) {
		if (size >= buffer.size())
			throw SpawnPayloadTooLargeError();

		buffer[size++] = value;
	}

	void WriteU8(uint8_t value) {
		WriteByte(static_cast<std::byte>(value));
	}

	void WriteBool(bool value) {
		WriteByte(static_cast<std::byte>(value));
	}

	void Write(SpawnExecCommand cmd) {
		WriteByte(static_cast<std::byte>(cmd));
	}

	void WriteOptional(SpawnExecCommand cmd, bool value) {
		if (value)
			Write(cmd);
	}

	void Write(std::span<const std::byte> value) {
		if (size + value.size() > buffer.size())
			throw SpawnPayloadTooLargeError();

		std::copy(value.begin(), value.end(),
			  std::next(buffer.begin(), size));
		size += value.size();
	}

	template<typename T>
	void Write(std::span<const T> value) noexcept {
		Write(std::as_bytes(value));
	}

	template<typename T>
	void WriteT(const T &value) {
		Write(std::span{&value, 1});
	}

	void WriteInt(int value) {
		WriteT(value);
	}

	void WriteUnsigned(unsigned value) {
		WriteT(value);
	}

	void WriteString(std::string_view value) {
		Write(AsBytes(value));
		WriteU8(0);
	}

	void WriteString(SpawnExecCommand cmd, std::string_view value) {
		Write(cmd);
		WriteString(value);
	}

	void WriteOptionalString(SpawnExecCommand cmd, const char *value) {
		if (value != nullptr)
			WriteString(cmd, value);
	}

	void WriteFd(SpawnExecCommand cmd, FileDescriptor fd) {
		assert(fd.IsDefined());

		if (fds.full())
			throw SpawnPayloadTooLargeError();

		Write(cmd);
		fds.push_back(fd);
	}

	void CheckWriteFd(SpawnExecCommand cmd, FileDescriptor fd) {
		if (fd.IsDefined())
			WriteFd(cmd, fd);
	}

	std::span<const std::byte> GetPayload() const {
		return {buffer.data(), size};
	}

	std::span<const FileDescriptor> GetFds() const noexcept {
		return fds;
	}
};

template<size_t MAX_FDS>
static void
Send(SocketDescriptor s, std::span<const std::byte> payload,
     std::span<const FileDescriptor> fds)
{
	assert(s.IsDefined());

	const std::array vec{MakeIovec(payload)};

	MessageHeader msg{std::span{vec}};

	ScmRightsBuilder<MAX_FDS> b(msg);
	for (const auto &i : fds)
		b.push_back(i.Get());
	b.Finish(msg);

	SendMessage(s, msg, MSG_NOSIGNAL);
}

template<size_t MAX_FDS>
static void
Send(SocketDescriptor socket, const SpawnSerializer &s)
{
	return Send<MAX_FDS>(socket, s.GetPayload(), s.GetFds());
}

#endif
