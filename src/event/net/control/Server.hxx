// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/control/Protocol.hxx"
#include "event/net/UdpHandler.hxx"
#include "event/net/UdpListener.hxx"

class SocketAddress;
class UniqueSocketDescriptor;
class EventLoop;
struct SocketConfig;

namespace BengControl {

class Handler;

/**
 * Server side part of the "control" protocol.
 */
class Server final : UdpHandler {
	Handler &handler;

	UdpListener socket;

public:
	Server(EventLoop &event_loop, UniqueSocketDescriptor s,
	       Handler &_handler) noexcept;

	Server(EventLoop &event_loop, Handler &_handler,
	       const SocketConfig &config);

	auto &GetEventLoop() const noexcept {
		return socket.GetEventLoop();
	}

	void Enable() noexcept {
		socket.Enable();
	}

	void Disable() noexcept {
		socket.Disable();
	}

	/**
	 * Throws std::runtime_error on error.
	 */
	void Reply(SocketAddress address,
		   Command command,
		   std::span<const std::byte> payload);

private:
	/* virtual methods from class UdpHandler */
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr ep) noexcept override;
};

} // namespace BengControl
