// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/SocketEvent.hxx"

#include <exception>

class PingClientHandler {
public:
	virtual void PingResponse() noexcept = 0;
	virtual void PingError(std::exception_ptr ep) noexcept = 0;
};

/**
 * Sends a "ping" (ICMP echo-request) to the server, and waits for the
 * reply.
 */
class PingClient final {
	SocketEvent event;

	PingClientHandler &handler;

	uint16_t ident;

public:
	PingClient(EventLoop &event_loop,
		   PingClientHandler &_handler) noexcept;

	~PingClient() noexcept {
		event.Close();
	}

	/**
	 * Is the #PingClient class available?
	 */
	[[gnu::const]]
	static bool IsAvailable() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Start(SocketAddress address) noexcept;

	void Cancel() noexcept {
		event.Close();
	}

private:
	void ScheduleRead() noexcept;
	void EventCallback(unsigned events) noexcept;

	void Read() noexcept;
};
