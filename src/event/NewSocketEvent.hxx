/*
 * Copyright 2007-2018 Content Management AG
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

#ifndef NEW_SOCKET_EVENT_HXX
#define NEW_SOCKET_EVENT_HXX

#include "SocketEvent.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/BindMethod.hxx"

class NewSocketEvent {
	SocketEvent read_event, write_event;

	using Callback = BoundMethod<void(unsigned events)>;

public:
	static constexpr unsigned READ = SocketEvent::READ;
	static constexpr unsigned WRITE = SocketEvent::WRITE;

	NewSocketEvent(EventLoop &event_loop, Callback callback,
		       SocketDescriptor fd=SocketDescriptor::Undefined()) noexcept
		:read_event(event_loop, fd.Get(),
			    SocketEvent::READ|SocketEvent::PERSIST,
			    callback),
		 write_event(event_loop, fd.Get(),
			     SocketEvent::WRITE|SocketEvent::PERSIST,
			     callback) {}

	~NewSocketEvent() noexcept {
		Cancel();
	}

	EventLoop &GetEventLoop() noexcept {
		return read_event.GetEventLoop();
	}

	gcc_pure
	SocketDescriptor GetSocket() const noexcept {
		return SocketDescriptor(read_event.GetFd());
	}

	void Open(SocketDescriptor fd) noexcept;

	unsigned GetScheduledFlags() const noexcept {
		return (READ * IsReadPending()) |
			(WRITE * IsWritePending());
	}

	void Schedule(unsigned flags) noexcept;

	void Cancel() noexcept {
		Schedule(0);
	}

	void ScheduleRead() noexcept {
		read_event.Add();
	}

	void ScheduleWrite() noexcept {
		write_event.Add();
	}

	void CancelRead() noexcept {
		read_event.Delete();
	}

	void CancelWrite() noexcept {
		write_event.Delete();
	}

	bool IsReadPending() const noexcept {
		return read_event.IsPending(SocketEvent::READ);
	}

	bool IsWritePending() const noexcept {
		return write_event.IsPending(SocketEvent::WRITE);
	}
};

#endif
