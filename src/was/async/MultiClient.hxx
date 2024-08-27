// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/SocketEvent.hxx"

#include <exception>

struct WasSocket;
class UniqueSocketDescriptor;

namespace Was {

class MultiClientHandler {
public:
	virtual void OnMultiClientDisconnect() noexcept = 0;
	virtual void OnMultiClientError(std::exception_ptr error) noexcept = 0;
};

class MultiClient {
	SocketEvent event;

	MultiClientHandler &handler;

public:
	MultiClient(EventLoop &event_loop, UniqueSocketDescriptor socket,
		    MultiClientHandler &_handler) noexcept;

	~MultiClient() noexcept {
		event.Close();
	}

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	/**
	 * Open a new WAS connection.
	 *
	 * This method is non-blocking; if the socket buffer is full
         * (EAGAIN), an exception is thrown.  Usually, the socket
         * buffer is large enough for this to never happen, unless the
         * load is extremely high, and then it's better to fail than
         * to add more load.
	 *
	 * @return a #WasSocket with input/output in non-blocking mode
	 */
	WasSocket Connect();

private:
	void OnSocketReady(unsigned events) noexcept;

	void Connect(WasSocket &&socket);
};

} // namespace Was
