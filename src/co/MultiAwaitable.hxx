// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Task.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>

namespace Co {

/**
 * A task that can be awaited on by multiple waiters.
 */
class MultiAwaitable final {
	bool ready = false;

	struct Request final : IntrusiveListHook<IntrusiveHookMode::TRACK> {
		MultiAwaitable &multi;

		std::coroutine_handle<> continuation;

		explicit Request(MultiAwaitable &_multi) noexcept
			:multi(_multi)
		{
			multi.AddRequest(*this);
		}

		~Request() noexcept {
			if (is_linked())
				multi.RemoveRequest(*this);
		}

		bool IsReady() const noexcept {
			return multi.IsReady();
		}
	};

	struct Awaitable final {
		std::unique_ptr<Request> request;

		Awaitable(MultiAwaitable &multi) noexcept
			:request(multi.IsReady() ? nullptr : new Request(multi)) {}

		bool await_ready() const noexcept {
			return !request || request->IsReady();
		}

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) noexcept {
			request->continuation = _continuation;
			return std::noop_coroutine();
		}

		void await_resume() noexcept {
		}
	};

	IntrusiveList<Request> requests;

	EagerTask<void> task;

public:
	template<typename T>
	explicit MultiAwaitable(T &&_task) noexcept
		:task(Wait(std::forward<T>(_task))) {}

	auto operator co_await() noexcept {
		return Awaitable{*this};
	}

private:
	bool IsReady() const noexcept {
		return ready;
	}

	void SetReady() noexcept {
		assert(!ready);
		ready = true;

		requests.clear_and_dispose([](Request *r){
			r->continuation.resume();
		});
	}

	EagerTask<void> Wait(auto _task) noexcept {
		co_await _task;
		SetReady();
	}

	void AddRequest(Request &r) noexcept {
		requests.push_back(r);
	}

	void RemoveRequest(Request &r) noexcept {
		requests.erase(requests.iterator_to(r));

		if (requests.empty())
			/* cancel the task */
			task = {};
	}
};

} // namespace Co
