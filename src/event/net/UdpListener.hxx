// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/SocketEvent.hxx"

#include <cstddef>
#include <span>

class UniqueSocketDescriptor;
class SocketAddress;
class UdpHandler;

/**
 * Listener on a UDP port.
 */
class UdpListener {
	SocketEvent event;

	UdpHandler &handler;

public:
	UdpListener(EventLoop &event_loop, UniqueSocketDescriptor _fd,
		    UdpHandler &_handler) noexcept;
	~UdpListener() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	bool IsDefined() const noexcept {
		return event.IsDefined();
	}

	/**
	 * Close the socket and disable this listener permanently.
	 */
	void Close() noexcept {
		event.Close();
	}

	/**
	 * Enable the object after it has been disabled by Disable().  A
	 * new object is enabled by default.
	 */
	void Enable() noexcept {
		event.ScheduleRead();
	}

	/**
	 * Disable the object temporarily.  To undo this, call Enable().
	 */
	void Disable() noexcept {
		event.Cancel();
	}

	/**
	 * Obtains the underlying socket, which can be used to send
	 * replies.
	 */
	SocketDescriptor GetSocket() const noexcept {
		return event.GetSocket();
	}

	/**
	 * Send a reply datagram to a client.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Reply(SocketAddress address, std::span<const std::byte> payload);

	/**
	 * Receive all pending datagram from the stocket and pass them
	 * to the handler (until the handler returns false).  Throws
	 * exception on error.
	 *
	 * @return false if one UdpHandler::OnUdpDatagram()
	 * invocation has returned false
	 */
	bool ReceiveAll();

private:
	/**
	 * Receive one datagram and pass it to the handler.  Throws
	 * exception on error.
	 *
	 * @return UdpHandler::OnUdpDatagram()
	 */
	bool ReceiveOne();

	void EventCallback(unsigned events) noexcept;
};
