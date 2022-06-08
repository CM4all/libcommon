/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
			listeners.front().Listen(UniqueSocketDescriptor(SD_LISTEN_FDS_START + i));
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
