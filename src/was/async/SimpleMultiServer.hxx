// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/net/UdpListener.hxx"
#include "event/net/UdpHandler.hxx"

struct WasSocket;
class UniqueSocketDescriptor;

namespace Was {

class SimpleMultiServer;

class SimpleMultiServerHandler {
public:
	virtual void OnMultiWasNew(SimpleMultiServer &server,
				   WasSocket &&socket) noexcept = 0;
	virtual void OnMultiWasError(SimpleMultiServer &server,
				     std::exception_ptr error) noexcept = 0;
	virtual void OnMultiWasClosed(SimpleMultiServer &server) noexcept = 0;
};

/**
 * A "simple" WAS server connection.
 */
class SimpleMultiServer final
	: UdpHandler
{
	UdpListener socket;

	SimpleMultiServerHandler &handler;

public:
	SimpleMultiServer(EventLoop &event_loop,
			  UniqueSocketDescriptor _socket,
			  SimpleMultiServerHandler &_handler) noexcept;

	auto &GetEventLoop() const noexcept {
		return socket.GetEventLoop();
	}

private:
	/* virtual methods from class UdpHandler */
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;

	bool OnUdpHangup() override {
		handler.OnMultiWasClosed(*this);
		return false;
	}

	void OnUdpError(std::exception_ptr e) noexcept override {
		handler.OnMultiWasError(*this, std::move(e));
	}
};

} // namespace Was
