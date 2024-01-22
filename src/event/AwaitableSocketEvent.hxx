// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SocketEvent.hxx"
#include "co/AwaitableHelper.hxx"

/**
 * A helper class that makes #SocketEvent awaitable by a coroutine.
 */
class AwaitableSocketEvent final {
	SocketEvent event;

	std::coroutine_handle<> continuation;

	unsigned events;

	using Awaitable = Co::AwaitableHelper<AwaitableSocketEvent, false>;
	friend Awaitable;

public:
	AwaitableSocketEvent(EventLoop &event_loop, SocketDescriptor socket,
			     unsigned flags) noexcept
		:event(event_loop, BIND_THIS_METHOD(OnSocketReady), socket)
	{
		event.Schedule(flags);
	}

	Awaitable operator co_await() noexcept {
		return *this;
	}

private:
	bool IsReady() const noexcept {
		return event.GetScheduledFlags() == 0;
	}

	unsigned TakeValue() noexcept {
		return events;
	}

	void OnSocketReady(unsigned _events) noexcept {
		events = _events;
		event.Cancel();

		if (continuation)
			continuation.resume();
	}
};
