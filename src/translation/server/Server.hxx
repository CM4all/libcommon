// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <forward_list>

class EventLoop;

namespace Translation::Server {

class Handler;
class Listener;

class Server final {
	std::forward_list<Listener> listeners;

public:
	/**
	 * Create a listener for the default address.  Uses systemd
	 * sockets instead of this process was launched by socket
	 * activation.
	 */
	Server(EventLoop &_event_loop, Handler &_handler);

	~Server() noexcept;
};

} // namespace Translation::Server
