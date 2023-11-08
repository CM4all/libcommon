// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Compat.hxx"
#include "AwaitableHelper.hxx"
#include "event/FineTimerEvent.hxx"

namespace Co {

/**
 * Put a coroutine to sleep by suspending it on `co_await` and
 * resuming it as soon as the #FineTimerEvent fires.
 */
class Sleep final {
	FineTimerEvent event;

	std::coroutine_handle<> continuation;

	using Awaitable = AwaitableHelper<Sleep, false>;
	friend Awaitable;

public:
	[[nodiscard]]
	Sleep(EventLoop &event_loop, Event::Duration d) noexcept
		:event(event_loop, BIND_THIS_METHOD(OnTimer))
	{
		event.Schedule(d);
	}

	[[nodiscard]]
	Awaitable operator co_await() noexcept {
		return *this;
	}

private:
	bool IsReady() const noexcept {
		return !event.IsPending();
	}

	void TakeValue() const noexcept {}

	void OnTimer() noexcept {
		continuation.resume();
	}
};

} // namespace Co

