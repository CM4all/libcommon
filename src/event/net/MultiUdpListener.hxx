// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/SocketEvent.hxx"
#include "net/MultiReceiveMessage.hxx"

#include <cstddef>

class UniqueSocketDescriptor;
class SocketAddress;
class UdpHandler;

/**
 * Listener on a UDP port.  Unlike #UdpListener, it uses recvmmsg()
 * for improved efficiency.
 */
class MultiUdpListener {
	SocketEvent event;

	MultiReceiveMessage multi;

	UdpHandler &handler;

public:
	MultiUdpListener(EventLoop &event_loop, UniqueSocketDescriptor _socket,
			 MultiReceiveMessage &&_multi,
			 UdpHandler &_handler) noexcept;
	~MultiUdpListener() noexcept;

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

private:
	void EventCallback(unsigned events) noexcept;
};
