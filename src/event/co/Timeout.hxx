// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/CoarseTimerEvent.hxx"
#include "net/TimeoutError.hxx"

#include <cassert>
#include <coroutine>
#include <optional>
#include <utility>

namespace Co {

/**
 * Wrap a task and throw #TimeoutError if it does not resume after a
 * certain amount of time.
 */
template<typename InnerTask>
class Timeout final {
	using InnerAwaitable = decltype(std::declval<InnerTask>().operator co_await());

	std::optional<InnerTask> inner_task;

	CoarseTimerEvent timer;

	std::coroutine_handle<> continuation;

	class Awaitable {
		Timeout &task;

		[[no_unique_address]]
		InnerAwaitable inner_awaitable;

	public:
		constexpr Awaitable(Timeout &_task, InnerAwaitable &&_inner) noexcept
			:task(_task), inner_awaitable(std::move(_inner))
		{
			assert(!task.HasTimedOut());
		}

		[[nodiscard]]
		constexpr bool await_ready() const noexcept {
			assert(task.inner_task);
			assert(!task.timer.IsPending());

			/* do not check timeout here because this will
			   only be called before the inner task is
			   suspended and thus no timeout can have
			   occurred yet */
			return inner_awaitable.await_ready();
		}

		decltype(auto) await_suspend(std::coroutine_handle<> _continuation) noexcept {
			assert(task.inner_task);
			assert(!task.timer.IsPending());

			/* arm the timer that has already been prepared by the
			   constructor */
			task.timer.ScheduleCurrent();

			/* save the continuation just in case we need
			   to resume it after a timeout; if all goes
			   well (i.e. no timeout), the inner task will
			   resume it, so pass it there as well */
			task.continuation = _continuation;
			return inner_awaitable.await_suspend(_continuation);
		}

		decltype(auto) await_resume() {
			if (task.HasTimedOut())
				throw TimeoutError{};

			assert(task.inner_task);

			task.timer.Cancel();

			return inner_awaitable.await_resume();
		}
	};

public:
	Timeout(EventLoop &event_loop, Event::Duration timeout,
		InnerTask &&_inner_task) noexcept
		:inner_task(std::move(_inner_task)),
		 timer(event_loop, BIND_THIS_METHOD(OnTimeout))
	{
		timer.SetDue(timeout);
	}

	template<typename... Args>
	Timeout(EventLoop &event_loop, Event::Duration timeout,
		std::in_place_t, Args&&... args) noexcept
		:inner_task(std::in_place, std::forward<Args>(args)...),
		 timer(event_loop, BIND_THIS_METHOD(OnTimeout))
	{
		timer.SetDue(timeout);
	}

	[[nodiscard]]
	Awaitable operator co_await() noexcept {
		assert(inner_task);
		assert(!timer.IsPending());

		/* wrap the inner task's awaitable */
		return {*this, inner_task->operator co_await()};
	}

private:
	bool WasSuspended() const noexcept {
		return static_cast<bool>(continuation);
	}

	bool HasTimedOut() const noexcept {
		return WasSuspended() && !timer.IsPending();
	}

	void OnTimeout() noexcept {
		assert(inner_task);
		assert(WasSuspended());

		/* cancel the inner task */
		inner_task.reset();

		if (continuation)
			continuation.resume();
	}
};

} // namespace Co

