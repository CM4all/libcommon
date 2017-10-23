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

#include "io/FdType.hxx"
#include "event/SocketEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "net/SocketDescriptor.hxx"

#include <cstddef>

#include <sys/types.h>
#include <assert.h>
#include <stdint.h>

template<typename T> class ForeignFifoBuffer;
class UniqueSocketDescriptor;

class SocketHandler {
public:
	/**
	 * The socket is ready for reading.
	 *
	 * @return false when the socket has been closed
	 */
	virtual bool OnSocketRead() noexcept = 0;

	/**
	 * The socket is ready for writing.
	 *
	 * @return false when the socket has been closed
	 */
	virtual bool OnSocketWrite() noexcept = 0;

	/**
	 * @return false when the socket has been closed
	 */
	virtual bool OnSocketTimeout() noexcept = 0;

	/**
	 * The peer has closed the socket.  There may still be data
	 * pending in the kernel socket buffer that can be received
	 * into userspace.
	 *
	 * @return false if the #SocketWrapper has been closed
	 */
	virtual bool OnSocketHangup() noexcept {
		return true;
	}

	/**
	 * An error has occurred (via EPOLLERR / SO_ERROR).
	 *
	 * @param error an errno value
	 * @return false if the #SocketWrapper has been closed
	 */
	virtual bool OnSocketError(int error) noexcept = 0;
};

class SocketWrapper {
	FdType fd_type;

	SocketEvent socket_event;
	CoarseTimerEvent read_timeout_event, write_timeout_event;

	SocketHandler &handler;

public:
	SocketWrapper(EventLoop &event_loop, SocketHandler &_handler) noexcept
		:socket_event(event_loop, BIND_THIS_METHOD(SocketEventCallback)),
		 read_timeout_event(event_loop, BIND_THIS_METHOD(TimeoutCallback)),
		 write_timeout_event(event_loop, BIND_THIS_METHOD(TimeoutCallback)),
		 handler(_handler) {}

	SocketWrapper(const SocketWrapper &) = delete;

	EventLoop &GetEventLoop() const noexcept {
		return socket_event.GetEventLoop();
	}

	void Init(SocketDescriptor _fd, FdType _fd_type) noexcept;
	void Init(UniqueSocketDescriptor _fd, FdType _fd_type) noexcept;

	/**
	 * Shut down the socket gracefully, allowing the TCP stack to
	 * complete all pending transfers.  If you call Close() without
	 * Shutdown(), it may reset the connection and discard pending
	 * data.
	 */
	void Shutdown() noexcept;

	void Close() noexcept;

	/**
	 * Just like Close(), but do not actually close the
	 * socket.  The caller is responsible for closing the socket (or
	 * scheduling it for reuse).
	 */
	void Abandon() noexcept;

	/**
	 * Returns the socket descriptor and calls socket_wrapper_abandon().
	 */
	int AsFD() noexcept;

	bool IsValid() const noexcept {
		return socket_event.IsDefined();
	}

	SocketDescriptor GetSocket() const noexcept {
		return socket_event.GetSocket();
	}

	FdType GetType() const noexcept {
		return fd_type;
	}

	/**
	 * @param timeout the read timeout; a negative value disables
	 * the timeout
	 */
	void ScheduleRead(Event::Duration timeout) noexcept {
		assert(IsValid());

		socket_event.ScheduleRead();

		if (timeout < timeout.zero())
			read_timeout_event.Cancel();
		else
			read_timeout_event.Schedule(timeout);
	}

	void UnscheduleRead() noexcept {
		socket_event.CancelRead();
		read_timeout_event.Cancel();
	}

	/**
	 * @param timeout the write timeout; a negative value disables
	 * the timeout
	 */
	void ScheduleWrite(Event::Duration timeout) noexcept {
		assert(IsValid());

		socket_event.ScheduleWrite();

		if (timeout < timeout.zero())
			write_timeout_event.Cancel();
		else
			write_timeout_event.Schedule(timeout);
	}

	void UnscheduleWrite() noexcept {
		socket_event.CancelWrite();
		write_timeout_event.Cancel();
	}

	[[gnu::pure]]
	bool IsReadPending() const noexcept {
		return socket_event.IsReadPending();
	}

	[[gnu::pure]]
	bool IsWritePending() const noexcept {
		return socket_event.IsWritePending();
	}

	ssize_t ReadToBuffer(ForeignFifoBuffer<uint8_t> &buffer) noexcept;

	[[gnu::pure]]
	bool IsReadyForWriting() const noexcept;

	ssize_t Write(const void *data, std::size_t length) noexcept;

	ssize_t WriteV(const struct iovec *v, std::size_t n) noexcept;

	ssize_t WriteFrom(int other_fd, FdType other_fd_type,
			  std::size_t length) noexcept;

private:
	void SocketEventCallback(unsigned events) noexcept;
	void TimeoutCallback() noexcept;
};
