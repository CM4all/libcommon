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
	SocketDescriptor GetSocket() noexcept {
		return event.GetSocket();
	}

	/**
	 * Send a reply datagram to a client.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Reply(SocketAddress address,
		   const void *data, std::size_t data_length);

private:
	void EventCallback(unsigned events) noexcept;
};
