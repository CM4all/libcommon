// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Listener.hxx"
#include "Connection.hxx"
#include "net/SocketAddress.hxx"
#include "io/Logger.hxx"
#include "util/DeleteDisposer.hxx"

namespace Translation::Server {

Listener::Listener(EventLoop &_event_loop, Handler &_handler) noexcept
	:ServerSocket(_event_loop),
	 handler(_handler)
{
}

Listener::~Listener() noexcept
{
	connections.clear_and_dispose(DeleteDisposer());
}

void
Listener::OnAccept(UniqueSocketDescriptor new_fd,
		   SocketAddress) noexcept
{
	auto *connection = new Connection(GetEventLoop(),
					  handler, std::move(new_fd));
	connections.push_back(*connection);
}

void
Listener::OnAcceptError(std::exception_ptr ep) noexcept
{
	LogConcat(2, "ts", ep);
}

} // namespace Translation::Server
