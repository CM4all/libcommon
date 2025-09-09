// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "net/control/Protocol.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <exception>
#include <span>

class UniqueFileDescriptor;
class SocketAddress;

namespace BengControl {

class Handler {
public:
	virtual void OnControlPacket(Command command,
				     std::span<const std::byte> payload,
				     std::span<UniqueFileDescriptor> fds,
				     SocketAddress address, int uid) = 0;

	virtual void OnControlError(std::exception_ptr &&error) noexcept = 0;
};

} // namespace BengControl
