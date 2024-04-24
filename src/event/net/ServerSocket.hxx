// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/UniqueSocketDescriptor.hxx"
#include "event/SocketEvent.hxx"

#include <exception>

class SocketAddress;

/**
 * A socket that accepts incoming connections.
 */
class ServerSocket {
	SocketEvent event;

public:
	explicit ServerSocket(EventLoop &event_loop) noexcept
		:event(event_loop, BIND_THIS_METHOD(EventCallback)) {}

	ServerSocket(EventLoop &event_loop,
		     UniqueSocketDescriptor _fd) noexcept
		:ServerSocket(event_loop) {
		Listen(std::move(_fd));
	}

	~ServerSocket() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Listen(UniqueSocketDescriptor _fd) noexcept;

	/**
	 * Throws on error.
	 */
	void Listen(SocketAddress address,
		    bool reuse_port=false,
		    bool free_bind=false,
		    const char *bind_to_device=nullptr);

	void ListenTCP(unsigned port);
	void ListenTCP4(unsigned port);
	void ListenTCP6(unsigned port);

	void ListenPath(const char *path);

	SocketDescriptor GetSocket() const noexcept {
		return event.GetSocket();
	}

protected:
	/**
	 * A new incoming connection has been established.
	 *
	 * @param fd the socket owned by the callee
	 */
	virtual void OnAccept(UniqueSocketDescriptor fd,
			      SocketAddress address) noexcept = 0;
	virtual void OnAcceptError(std::exception_ptr ep) noexcept = 0;

private:
	void EventCallback(unsigned events) noexcept;
};
