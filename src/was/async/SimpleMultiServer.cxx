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
				      {UniqueSocketDescriptor{fds[0].Release()}, std::move(fds[1]), std::move(fds[2])});
		break;
	}

	return true;
}

} // namespace Was
