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
class ControlServer;

class ControlHandler {
public:
	virtual void OnControlPacket(ControlServer &control_server,
				     BengProxy::ControlCommand command,
				     std::span<const std::byte> payload,
				     std::span<UniqueFileDescriptor> fds,
				     SocketAddress address, int uid) = 0;

	virtual void OnControlError(std::exception_ptr ep) noexcept = 0;
};
