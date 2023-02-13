// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/net/ServerSocket.hxx"
#include "util/IntrusiveList.hxx"

namespace Translation::Server {

class Handler;
class Connection;

class Listener final : private ServerSocket {
	Handler &handler;

	IntrusiveList<Connection> connections;

public:
	Listener(EventLoop &_event_loop, Handler &_handler) noexcept;
	~Listener() noexcept;

	using ServerSocket::GetEventLoop;
	using ServerSocket::Listen;
	using ServerSocket::ListenPath;

private:
	void OnAccept(UniqueSocketDescriptor &&fd,
		      SocketAddress address) noexcept override;
	void OnAcceptError(std::exception_ptr ep) noexcept override;
};

} // namespace Translation::Server
