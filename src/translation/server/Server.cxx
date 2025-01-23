// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Server.hxx"
#include "Listener.hxx"

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

namespace Translation::Server {

Server::Server(EventLoop &_event_loop, Handler &_handler)
{
#ifdef HAVE_LIBSYSTEMD
	int n = sd_listen_fds(true);
	if (n > 0) {
		/* systemd has launched us by socket activation; use
		   those sockets */
		for (unsigned i = 0; i < unsigned(n); ++i) {
			listeners.emplace_front(_event_loop, _handler);
			listeners.front().Listen(UniqueSocketDescriptor{
				AdoptTag{},
				static_cast<int>(SD_LISTEN_FDS_START + i),
			});
		}

		/* ... instead of the default socket; we're done
		   now */
		return;
	}
#endif

	listeners.emplace_front(_event_loop, _handler);
	listeners.front().ListenPath("@translation");
}

Server::~Server() noexcept = default;

} // namespace Translation::Server
