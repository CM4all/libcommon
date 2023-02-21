// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "co/Compat.hxx"

#include <cassert>

namespace Co {

class PauseTask {
	std::coroutine_handle<> continuation;

	bool resumed = false;

	struct Awaitable final {
		PauseTask &task;

		Awaitable(PauseTask &_task) noexcept:task(_task) {}

		~Awaitable() noexcept {
			task.continuation = {};
		}

		Awaitable(const Awaitable &) = delete;
		Awaitable &operator=(const Awaitable &) = delete;

		bool await_ready() const noexcept {
			return task.resumed;
		}

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) noexcept {
			task.continuation = _continuation;
			return std::noop_coroutine();
		}

		void await_resume() noexcept {
		}
	};

public:
	Awaitable operator co_await() noexcept {
		return {*this};
	}

	bool IsAwaited() const noexcept {
		return (bool)continuation;
	}

	void Resume() noexcept {
		assert(!resumed);

		resumed = true;

		if (continuation)
			continuation.resume();
	}
};

} // namespace Co
