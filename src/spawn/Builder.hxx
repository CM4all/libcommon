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

#ifndef BENG_PROXY_SPAWN_BUILDER_HXX
#define BENG_PROXY_SPAWN_BUILDER_HXX

#include "net/SocketDescriptor.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "net/SendMessage.hxx"
#include "IProtocol.hxx"
#include "system/Error.hxx"
#include "io/FileDescriptor.hxx"
#include "io/Iovec.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StaticArray.hxx"

#include <cstddef>

#include <assert.h>
#include <stdint.h>
#include <string.h>

class SpawnPayloadTooLargeError {};

class SpawnSerializer {
	static constexpr size_t capacity = 65536;

	size_t size = 0;

	std::byte buffer[capacity];

	StaticArray<FileDescriptor, 8> fds;

public:
	explicit SpawnSerializer(SpawnRequestCommand cmd) {
		buffer[size++] = static_cast<std::byte>(cmd);
	}

	explicit SpawnSerializer(SpawnResponseCommand cmd) {
		buffer[size++] = static_cast<std::byte>(cmd);
	}

	void WriteByte(std::byte value) {
		if (size >= capacity)
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

	void Write(ConstBuffer<void> value) {
		if (size + value.size > capacity)
			throw SpawnPayloadTooLargeError();

		memcpy(buffer + size, value.data, value.size);
		size += value.size;
	}

	template<typename T>
	void WriteT(const T &value) {
		Write(ConstBuffer<void>(&value, sizeof(value)));
	}

	void WriteInt(int value) {
		WriteT(value);
	}

	void WriteString(const char *value) {
		assert(value != nullptr);

		Write(ConstBuffer<void>(value, strlen(value) + 1));
	}

	void WriteString(SpawnExecCommand cmd, const char *value) {
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

	ConstBuffer<void> GetPayload() const {
		return {buffer, size};
	}

	ConstBuffer<FileDescriptor> GetFds() const noexcept {
		return fds;
	}
};

template<size_t MAX_FDS>
static void
Send(SocketDescriptor s, ConstBuffer<void> payload,
     ConstBuffer<FileDescriptor> fds)
{
	assert(s.IsDefined());

	auto vec = MakeIovec(payload);

	MessageHeader msg(ConstBuffer<struct iovec>(&vec, 1));

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
