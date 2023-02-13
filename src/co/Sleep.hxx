// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Compat.hxx"
#include "event/FineTimerEvent.hxx"

namespace Co {

/**
 * Put a coroutine to sleep by suspending it on `co_await` and
 * resuming it as soon as the #FineTimerEvent fires.
 */
class Sleep final {
	FineTimerEvent event;

	std::coroutine_handle<> continuation;

public:
	Sleep(EventLoop &event_loop, Event::Duration d) noexcept
		:event(event_loop, BIND_THIS_METHOD(OnTimer))
	{
		event.Schedule(d);
	}

	auto operator co_await() noexcept {
		struct Awaitable final {
			Sleep &sleep;

			bool await_ready() const noexcept {
				return !sleep.event.IsPending();
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) noexcept {
				sleep.continuation = _continuation;
				return std::noop_coroutine();
			}

			void await_resume() noexcept {
			}
		};

		return Awaitable{*this};
	}

private:
	void OnTimer() noexcept {
		continuation.resume();
	}
};

} // namespace Co

