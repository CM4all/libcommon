// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Task.hxx"
#include "util/IntrusiveList.hxx"

#include <cassert>

namespace Co {

/**
 * A task that can be awaited on by multiple waiters.
 */
class MultiAwaitable final {
	bool ready = false;

	struct Awaitable final : IntrusiveListHook<IntrusiveHookMode::TRACK> {
		MultiAwaitable &multi;

		std::coroutine_handle<> continuation;

		bool ready;

		explicit Awaitable(MultiAwaitable &_multi) noexcept
			:multi(_multi), ready(multi.ready) {}

		~Awaitable() noexcept {
			if (is_linked()) {
				unlink();

				if (!ready)
					multi.CheckCancel();
			}
		}

		Awaitable(const Awaitable &) = delete;
		Awaitable &operator=(const Awaitable &) = delete;

		bool await_ready() const noexcept {
			assert(!is_linked());
			assert(!continuation);

			return ready;
		}

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) noexcept {
			assert(!is_linked());

			continuation = _continuation;

			multi.AddRequest(*this);

			return std::noop_coroutine();
		}

		void await_resume() noexcept {
			assert(!is_linked());
			assert(ready);
		}
	};

	IntrusiveList<Awaitable> requests;

	EagerTask<void> task;

public:
	template<typename T>
	explicit MultiAwaitable(T &&_task) noexcept
		:task(Wait(std::forward<T>(_task))) {}

	auto operator co_await() noexcept {
		return Awaitable{*this};
	}

private:
	void SetReady() noexcept {
		assert(!ready);
		ready = true;

		for (auto &i : requests) {
			assert(!i.ready);
			i.ready = true;
		}

		requests.clear_and_dispose([](Awaitable *r){
			r->continuation.resume();
		});
	}

	EagerTask<void> Wait(auto _task) noexcept {
		co_await _task;
		SetReady();
	}

	void AddRequest(Awaitable &r) noexcept {
		assert(!ready);

		requests.push_back(r);
	}

	void CheckCancel() noexcept {
		assert(!ready);

		if (requests.empty())
			/* cancel the task */
			task = {};
	}
};

} // namespace Co
