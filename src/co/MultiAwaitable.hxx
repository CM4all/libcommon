// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "UniqueHandle.hxx"
#include "util/IntrusiveList.hxx"

#include <cassert>

namespace Co {

/**
 * A task that can be awaited on by multiple waiters.
 *
 * This object must remain valid until all waiters have been resumed.
 */
class MultiAwaitable final {
	bool ready = false;

	struct Awaitable final : IntrusiveListHook<IntrusiveHookMode::TRACK> {
		MultiAwaitable &multi;

		std::coroutine_handle<> continuation;

		bool ready;

		[[nodiscard]]
		explicit Awaitable(MultiAwaitable &_multi) noexcept
			:multi(_multi), ready(multi.ready) {}

		~Awaitable() noexcept {
			if (is_linked()) {
				assert(continuation);
				assert(!continuation.done());

				unlink();

				if (!ready)
					multi.CheckCancel();
			}
		}

		Awaitable(const Awaitable &) = delete;
		Awaitable &operator=(const Awaitable &) = delete;

		[[nodiscard]]
		bool await_ready() const noexcept {
			assert(!is_linked());
			assert(!continuation);

			return ready;
		}

		void await_suspend(std::coroutine_handle<> _continuation) noexcept {
			assert(!is_linked());
			assert(!continuation);
			assert(_continuation);
			assert(!_continuation.done());

			continuation = _continuation;

			multi.AddRequest(*this);
		}

		void await_resume() noexcept {
			assert(!is_linked());
			assert(ready);
		}
	};

	struct MultiTask {
		struct promise_type {
			MultiTask *task;

			[[nodiscard]]
			auto initial_suspend() noexcept {
				return std::suspend_never{};
			}

			[[nodiscard]]
			auto final_suspend() noexcept {
				/* the coroutine_handle has already
				   been released by Wait() and the
				   coroutine handle will be freed by
				   our caller */

				return std::suspend_never{};
			}

			[[nodiscard]]
			auto get_return_object() noexcept {
				return MultiTask{std::coroutine_handle<promise_type>::from_promise(*this)};
			}

			[[noreturn]]
			void unhandled_exception() {
				/* the coroutine_handle will be
				   destroyed by the compiler after
				   rethrowing the exception */
				(void)task->coroutine.release();

				throw;
			}
		};

		UniqueHandle<promise_type> coroutine;

		[[nodiscard]]
		MultiTask() noexcept = default;

		[[nodiscard]]
		explicit MultiTask(std::coroutine_handle<promise_type> _coroutine) noexcept
			:coroutine(_coroutine) {
			coroutine->promise().task = this;
		}

		[[nodiscard]]
		auto release() noexcept {
			assert(coroutine);

			return coroutine.release();
		}
	};

	/**
	 * A list of suspended waiters.
	 */
	IntrusiveList<Awaitable> requests;

	/**
	 * This refers to coroutine Wait() which executes the actual
	 * task.
	 */
	MultiTask task;

public:
	/**
	 * Construct an instance without a task.  Call Start() to
	 * start a task.
	 */
	[[nodiscard]]
	MultiAwaitable() noexcept
		:ready(true) {}

	/**
	 * Construct an instance with a task.
	 */
	template<typename T>
	[[nodiscard]]
	explicit MultiAwaitable(T &&_task) noexcept
		:task(Wait(std::forward<T>(_task))) {}

	bool IsActive() const noexcept {
		return !ready;
	}

	/**
	 * Start a task.  This is only possible if no task is
	 * currently running.
	 */
	template<typename T>
	void Start(T &&_task) noexcept {
		assert(!IsActive());
		assert(requests.empty());

		ready = false;
		task = Wait(std::forward<T>(_task));
	}

	/**
	 * Creates a new awaitable
	 */
	[[nodiscard]]
	auto operator co_await() noexcept {
		return Awaitable{*this};
	}

private:
	void SetReady() noexcept {
		assert(!ready);
		ready = true;

		for (auto &i : requests) {
			assert(!i.ready);
			assert(i.continuation);
			assert(!i.continuation.done());

			i.ready = true;
		}

		/* move the request list to the stack just in case one
		   of the continuations deletes the MultiAwaitable */
		auto tmp = std::move(requests);

		tmp.clear_and_dispose([](Awaitable *r){
			assert(r->ready);
			assert(r->continuation);
			assert(!r->continuation.done());

			r->continuation.resume();
		});
	}

	/**
	 * A coroutine which executes the actual task and then resumes
	 * all waiters.
	 */
	[[nodiscard]]
	MultiTask Wait(auto _task) noexcept {
		assert(!ready);

		co_await _task;

		/* move the request list to the stack just in case one
		   of the continuations deletes the MultiAwaitable */
		auto this_task = std::move(task);

		SetReady();

		/* the coroutine_handle will be freed automatically */
		(void)this_task.release();
	}

	void AddRequest(Awaitable &r) noexcept {
		assert(!ready);

		requests.push_back(r);
	}

	void CheckCancel() noexcept {
		assert(!ready);

		if (requests.empty()) {
			/* cancel the task */
			ready = true;
			task = {};
		}
	}
};

} // namespace Co
