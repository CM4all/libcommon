// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

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

namespace Spawn {

class PayloadTooLargeError {};

class Serializer {
	size_t size = 0;

	std::array<std::byte, 65536> buffer;

	StaticVector<FileDescriptor, 8> fds;

public:
	explicit constexpr Serializer(RequestCommand cmd) noexcept {
		buffer[size++] = static_cast<std::byte>(cmd);
	}

	explicit constexpr Serializer(ResponseCommand cmd) noexcept {
		buffer[size++] = static_cast<std::byte>(cmd);
	}

	void WriteByte(std::byte value) {
		if (size >= buffer.size())
			throw PayloadTooLargeError();

		buffer[size++] = value;
	}

	void WriteU8(uint8_t value) {
		WriteByte(static_cast<std::byte>(value));
	}

	void WriteBool(bool value) {
		WriteByte(static_cast<std::byte>(value));
	}

	void Write(ExecCommand cmd) {
		WriteByte(static_cast<std::byte>(cmd));
	}

	void WriteOptional(ExecCommand cmd, bool value) {
		if (value)
			Write(cmd);
	}

	void Write(std::span<const std::byte> value) {
		if (size + value.size() > buffer.size())
			throw PayloadTooLargeError();

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

	void WriteString(ExecCommand cmd, std::string_view value) {
		Write(cmd);
		WriteString(value);
	}

	void WriteOptionalString(ExecCommand cmd, const char *value) {
		if (value != nullptr)
			WriteString(cmd, value);
	}

	void WriteFd(ExecCommand cmd, FileDescriptor fd) {
		assert(fd.IsDefined());

		if (fds.full())
			throw PayloadTooLargeError();

		Write(cmd);
		fds.push_back(fd);
	}

	void CheckWriteFd(ExecCommand cmd, FileDescriptor fd) {
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
Send(SocketDescriptor socket, const Serializer &s)
{
	return Send<MAX_FDS>(socket, s.GetPayload(), s.GetFds());
}

} // namespace Spawn
