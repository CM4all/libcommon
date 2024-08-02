// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SimpleMultiServer.hxx"
#include "Error.hxx"
#include "Socket.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <was/protocol.h>

namespace Was {

SimpleMultiServer::SimpleMultiServer(EventLoop &event_loop,
				     UniqueSocketDescriptor _socket,
				     SimpleMultiServerHandler &_handler) noexcept
	:socket(event_loop, std::move(_socket), *this),
	 handler(_handler)
{
}

bool
SimpleMultiServer::OnUdpDatagram(std::span<const std::byte> payload,
				 std::span<UniqueFileDescriptor> fds,
				 SocketAddress, int)
{
	const auto &h = *(const struct was_header *)(const void *)payload.data();

	if (payload.size() < sizeof(h) ||
	    payload.size() != sizeof(h) + h.length)
		throw WasProtocolError{"Malformed Multi-WAS datagram"};

	const auto cmd = (enum multi_was_command)h.command;

	switch (cmd) {
	case MULTI_WAS_COMMAND_NOP:
		break;

	case MULTI_WAS_COMMAND_NEW:
		if (fds.size() != 3 || h.length != 0)
			throw WasProtocolError{"Malformed Multi-WAS NEW datagram"};

		handler.OnMultiWasNew(*this,
				      {UniqueSocketDescriptor{std::move(fds[0])}, std::move(fds[1]), std::move(fds[2])});
		break;
	}

	return true;
}

} // namespace Was
