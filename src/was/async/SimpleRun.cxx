/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "SimpleRun.hxx"
#include "SimpleServer.hxx"
#include "event/Loop.hxx"

#include <unistd.h>

namespace Was {

class RunConnectionHandler final : public SimpleServerHandler {
	EventLoop &event_loop;

	std::exception_ptr error;

public:
	explicit RunConnectionHandler(EventLoop &_event_loop) noexcept
		:event_loop(_event_loop) {}

	void OnWasError(Was::SimpleServer &,
			std::exception_ptr _error) noexcept override {
		error = std::move(_error);
		event_loop.Break();
	}

	void OnWasClosed(Was::SimpleServer &) noexcept override {
		event_loop.Break();
	}

	void CheckRethrowError() const {
		if (error)
			std::rethrow_exception(error);
	}
};


void
Run(EventLoop &event_loop, SimpleRequestHandler &request_handler)
{
	RunConnectionHandler connection_handler(event_loop);
	Was::SimpleServer s(event_loop,
			    {
				    UniqueSocketDescriptor{3},
				    UniqueFileDescriptor(STDIN_FILENO),
				    UniqueFileDescriptor(STDOUT_FILENO),
			    },
			    connection_handler,
			    request_handler);

	event_loop.Dispatch();
	connection_handler.CheckRethrowError();
}

} // namespace Was
