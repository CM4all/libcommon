/*
 * Copyright 2017-2020 CM4all GmbH
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

#include "net/SocketDescriptor.hxx"
#include "util/BindMethod.hxx"
#include "util/IntrusiveList.hxx"

#include <sys/epoll.h>

class EventLoop;

class SocketEvent final : public IntrusiveListHook
{
	EventLoop &loop;

	using Callback = BoundMethod<void(unsigned events) noexcept>;
	const Callback callback;

	SocketDescriptor fd;

	/**
	 * A bit mask of events that are currently registered in the
	 * #EventLoop.
	 */
	unsigned scheduled_flags = 0;

	/**
	 * A bit mask of events which have been reported as "ready" by
	 * epoll_wait().  If non-zero, then the #EventLoop will call
	 * Dispatch() soon.
	 */
	unsigned ready_flags = 0;

public:
	static constexpr unsigned READ = EPOLLIN;
	static constexpr unsigned WRITE = EPOLLOUT;
	static constexpr unsigned ERROR = EPOLLERR;
	static constexpr unsigned HANGUP = EPOLLHUP;

	/**
	 * These flags are always reported by epoll_wait() and don't
	 * need to be registered with epoll_ctl().
	 */
	static constexpr unsigned IMPLICIT_FLAGS = ERROR|HANGUP;

	SocketEvent(EventLoop &_loop, Callback _callback,
		    SocketDescriptor _fd=SocketDescriptor::Undefined()) noexcept
		:loop(_loop),
		 callback(_callback),
		 fd(_fd) {}

	~SocketEvent() noexcept {
		Cancel();
	}

	SocketEvent(const SocketEvent &) = delete;
	SocketEvent &operator=(const SocketEvent &) = delete;

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	gcc_pure
	bool IsDefined() const noexcept {
		return fd.IsDefined();
	}

	gcc_pure
	SocketDescriptor GetSocket() const noexcept {
		return fd;
	}

	SocketDescriptor ReleaseSocket() noexcept {
		Cancel();
		return std::exchange(fd, SocketDescriptor::Undefined());
	}

	void Open(SocketDescriptor fd) noexcept;

	/**
	 * Close the socket (and cancel all scheduled events).
	 */
	void Close() noexcept;

	/**
	 * Call this instead of Cancel() to unregister this object
	 * after the underlying socket has already been closed.  This
	 * skips the `EPOLL_CTL_DEL` call because the kernel
	 * automatically removes closed file descriptors from epoll.
	 *
	 * Doing `EPOLL_CTL_DEL` on a closed file descriptor usually
	 * fails with `-EBADF` or could unregister a different socket
	 * which happens to be on the same file descriptor number.
	 */
	void Abandon() noexcept;

	unsigned GetScheduledFlags() const noexcept {
		return scheduled_flags;
	}

	void SetReadyFlags(unsigned flags) noexcept {
		ready_flags = flags;
	}

	/**
	 * @return true on success, false on error (with errno set)
	 */
	bool Schedule(unsigned flags) noexcept;

	void Cancel() noexcept {
		Schedule(0);
	}

	bool ScheduleRead() noexcept {
		return Schedule(GetScheduledFlags() | READ);
	}

	bool ScheduleWrite() noexcept {
		return Schedule(GetScheduledFlags() | WRITE);
	}

	void CancelRead() noexcept {
		Schedule(GetScheduledFlags() & ~READ);
	}

	void CancelWrite() noexcept {
		Schedule(GetScheduledFlags() & ~WRITE);
	}

	/**
	 * Schedule only the #IMPLICIT_FLAGS without #READ and #WRITE.
	 * This is not possible with Schedule(), and no other
	 * ScheduleX()/CancelX() method may be called on this object.
	 */
	void ScheduleImplicit() noexcept;

	bool IsReadPending() const noexcept {
		return GetScheduledFlags() & READ;
	}

	bool IsWritePending() const noexcept {
		return GetScheduledFlags() & WRITE;
	}

	/**
	 * Dispatch the events that were passed to SetReadyFlags().
	 */
	void Dispatch() noexcept;
};
