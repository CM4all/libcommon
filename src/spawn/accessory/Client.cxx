// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Client.hxx"
#include "Builder.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/SendMessage.hxx"
#include "util/CRC32.hxx"
#include "util/SpanCast.hxx"

#include <sched.h>

using std::string_view_literals::operator""sv;

static UniqueSocketDescriptor
CreateConnectLocalSocket(std::string_view path)
{
	UniqueSocketDescriptor s;
	if (!s.Create(AF_LOCAL, SOCK_SEQPACKET, 0))
		throw MakeErrno("Failed to create socket");

	if (!s.Connect(LocalSocketAddress{path}))
		throw FmtErrno("Failed to connect to {}", path);

	return s;
}

namespace SpawnAccessory {

/**
 * Connect to the local Spawn daemon.
 */
UniqueSocketDescriptor
Connect()
{
	return CreateConnectLocalSocket("@cm4all-spawn"sv);
}

static void
SendPidNamespaceRequest(SocketDescriptor s, std::string_view name)
{
	DatagramBuilder b;

	const RequestHeader name_header{uint16_t(name.size()), RequestCommand::NAME};
	b.Append(name_header);
	b.AppendPadded(AsBytes(name));

	static constexpr RequestHeader pid_namespace_header{0, RequestCommand::PID_NAMESPACE};
	b.Append(pid_namespace_header);

	SendMessage(s, b.Finish(), 0);
}

template<size_t PAYLOAD_SIZE, size_t CMSG_SIZE>
static std::pair<std::span<const std::byte>, std::vector<UniqueFileDescriptor>>
ReceiveDatagram(SocketDescriptor s,
		ReceiveMessageBuffer<PAYLOAD_SIZE, CMSG_SIZE> &buffer)
{
	auto response = ReceiveMessage(s, buffer, 0);
	auto payload = response.payload;
	const auto &dh = *(const DatagramHeader *)(const void *)payload.data();
	if (payload.size() < sizeof(dh))
		throw std::runtime_error("Response datagram too small");

	if (dh.magic != MAGIC)
		throw std::runtime_error("Wrong magic in response datagram");

	payload = payload.subspan(sizeof(dh));

	if (dh.crc != CRC32(payload))
		throw std::runtime_error("Bad CRC in response datagram");

	return {payload, std::move(response.fds)};
}

UniqueFileDescriptor
MakePidNamespace(SocketDescriptor s, const char *name)
{
	SendPidNamespaceRequest(s, name);

	ReceiveMessageBuffer<1024, 4> buffer;
	auto d = ReceiveDatagram(s, buffer);
	auto payload = d.first;
	auto &fds = d.second;

	const auto &rh = *(const ResponseHeader *)(const void *)payload.data();
	if (payload.size() < sizeof(rh))
		throw std::runtime_error("Response datagram too small");

	payload = payload.subspan(sizeof(rh));

	if (payload.size() < rh.size)
		throw std::runtime_error("Response datagram too small");

	switch (rh.command) {
	case ResponseCommand::ERROR:
		throw FmtRuntimeError("Spawn server error: {}",
				      ToStringView(payload));

	case ResponseCommand::NAMESPACE_HANDLES:
		if (rh.size != sizeof(uint32_t) ||
		    *(const uint32_t *)(const void *)payload.data() != CLONE_NEWPID ||
		    fds.size() != 1)
			throw std::runtime_error("Malformed NAMESPACE_HANDLES payload");

		return std::move(fds.front());
	}

	throw std::runtime_error("NAMESPACE_HANDLES expected");
}

}
