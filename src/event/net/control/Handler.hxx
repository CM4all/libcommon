// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/control/Protocol.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <exception>
#include <span>

class UniqueFileDescriptor;
class SocketAddress;

namespace BengControl {

class Server;

class Handler {
public:
	virtual void OnControlPacket(Server &control_server,
				     Command command,
				     std::span<const std::byte> payload,
				     std::span<UniqueFileDescriptor> fds,
				     SocketAddress address, int uid) = 0;

	virtual void OnControlError(std::exception_ptr ep) noexcept = 0;
};

} // namespace BengControl
