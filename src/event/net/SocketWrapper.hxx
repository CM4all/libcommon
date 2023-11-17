// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/FdType.hxx"
#include "event/SocketEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "net/SocketDescriptor.hxx"

#include <cassert>
#include <cstddef>

#include <sys/types.h> // for ssize_t

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
	CoarseTimerEvent write_timeout_event;

	SocketHandler &handler;

public:
	SocketWrapper(EventLoop &event_loop, SocketHandler &_handler) noexcept
		:socket_event(event_loop, BIND_THIS_METHOD(SocketEventCallback)),
		 write_timeout_event(event_loop, BIND_THIS_METHOD(TimeoutCallback)),
		 handler(_handler) {}

	SocketWrapper(const SocketWrapper &) = delete;

	~SocketWrapper() noexcept {
		Close();
	}

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

	void ScheduleRead() noexcept {
		assert(IsValid());

		socket_event.ScheduleRead();
	}

	void UnscheduleRead() noexcept {
		socket_event.CancelRead();
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

	ssize_t ReadToBuffer(ForeignFifoBuffer<std::byte> &buffer) noexcept;

	[[gnu::pure]]
	bool IsReadyForWriting() const noexcept;

	ssize_t Write(std::span<const std::byte> src) noexcept;

	ssize_t WriteV(std::span<const struct iovec> v) noexcept;

	ssize_t WriteFrom(FileDescriptor other_fd, FdType other_fd_type,
			  off_t *other_offset,
			  std::size_t length) noexcept;

private:
	void SocketEventCallback(unsigned events) noexcept;
	void TimeoutCallback() noexcept;
};
