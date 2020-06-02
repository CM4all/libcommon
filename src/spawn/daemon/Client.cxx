/*
 * Copyright 2017-2018 Content Management AG
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

#include "Client.hxx"
#include "Builder.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/SendMessage.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringView.hxx"

#include <boost/crc.hpp>

#include <sched.h>

static UniqueSocketDescriptor
CreateConnectLocalSocket(const char *path)
{
	UniqueSocketDescriptor s;
	if (!s.Create(AF_LOCAL, SOCK_SEQPACKET, 0))
		throw MakeErrno("Failed to create socket");

	{
		AllocatedSocketAddress address;
		address.SetLocal(path);
		if (!s.Connect(address))
			throw FormatErrno("Failed to connect to %s", path);
	}

	return s;
}

namespace SpawnDaemon {

/**
 * Connect to the local Spawn daemon.
 */
UniqueSocketDescriptor
Connect()
{
	return CreateConnectLocalSocket("@cm4all-spawn");
}

static void
SendPidNamespaceRequest(SocketDescriptor s, StringView name)
{
	DatagramBuilder b;

	const RequestHeader name_header{uint16_t(name.size), RequestCommand::NAME};
	b.Append(name_header);
	b.AppendPadded(name.ToVoid());

	static constexpr RequestHeader pid_namespace_header{0, RequestCommand::PID_NAMESPACE};
	b.Append(pid_namespace_header);

	SendMessage(s, b.Finish(), 0);
}

template<size_t PAYLOAD_SIZE, size_t CMSG_SIZE>
static std::pair<ConstBuffer<void>, std::vector<UniqueFileDescriptor>>
ReceiveDatagram(SocketDescriptor s,
		ReceiveMessageBuffer<PAYLOAD_SIZE, CMSG_SIZE> &buffer)
{
	auto response = ReceiveMessage(s, buffer, 0);
	auto payload = response.payload;
	const auto &dh = *(const DatagramHeader *)payload.data;
	if (payload.size < sizeof(dh))
		throw std::runtime_error("Response datagram too small");

	if (dh.magic != MAGIC)
		throw std::runtime_error("Wrong magic in response datagram");

	payload.data = &dh + 1;
	payload.size -= sizeof(dh);

	{
		boost::crc_32_type crc;
		crc.reset();
		crc.process_bytes(payload.data, payload.size);
		if (dh.crc != crc.checksum())
			throw std::runtime_error("Bad CRC in response datagram");
	}

	return std::make_pair(payload, std::move(response.fds));
}

UniqueFileDescriptor
MakePidNamespace(SocketDescriptor s, const char *name)
{
	SendPidNamespaceRequest(s, name);

	ReceiveMessageBuffer<1024, 4> buffer;
	auto d = ReceiveDatagram(s, buffer);
	auto payload = d.first;
	auto &fds = d.second;

	const auto &rh = *(const ResponseHeader *)payload.data;
	if (payload.size < sizeof(rh))
		throw std::runtime_error("Response datagram too small");

	payload.data = &rh + 1;
	payload.size -= sizeof(rh);

	if (payload.size < rh.size)
		throw std::runtime_error("Response datagram too small");

	switch (rh.command) {
	case ResponseCommand::ERROR:
		throw FormatRuntimeError("Spawn server error: %.*s",
					 int(rh.size),
					 (const char *)payload.data);

	case ResponseCommand::NAMESPACE_HANDLES:
		if (rh.size != sizeof(uint32_t) ||
		    *(const uint32_t *)payload.data != CLONE_NEWPID ||
		    fds.size() != 1)
			throw std::runtime_error("Malformed NAMESPACE_HANDLES payload");

		return std::move(fds.front());
	}

	throw std::runtime_error("NAMESPACE_HANDLES expected");
}

}
