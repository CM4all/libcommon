// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "co/AwaitableHelper.hxx"
#include "event/DeferEvent.hxx"

namespace Co {

/**
 * Resume in the next #EventLoop iteration.
 */
class Yield final {
	DeferEvent event;

	std::coroutine_handle<> continuation;

	using Awaitable = AwaitableHelper<Yield, false>;
	friend Awaitable;

public:
	[[nodiscard]]
	Yield(EventLoop &event_loop) noexcept
		:event(event_loop, BIND_THIS_METHOD(Resume))
	{
		event.Schedule();
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

	void Resume() noexcept {
		if (continuation)
			continuation.resume();
	}
};

} // namespace Co

