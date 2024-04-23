// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Resume.hxx"

#include <cassert>
#include <coroutine>

namespace Lua {

class Thread;

/**
 * A C++20 awaitable that resumes a Lua coroutine and waits for it to
 * complete.
 *
 * The constructor will resume the thread and the destructor cancels
 * and disposes it.
 */
class CoAwaitable final : ResumeListener {
	Thread &thread;

	std::exception_ptr error;

	std::coroutine_handle<> continuation;

	bool ready = false;

public:
	/**
	 * @param narg the number of arguments passed to the coroutine
	 */
	CoAwaitable(Thread &_thread, lua_State *thread_L, int narg);

	~CoAwaitable() noexcept;

	bool await_ready() const noexcept {
		return ready;
	}

	void await_suspend(std::coroutine_handle<> _continuation) noexcept {
		assert(!ready);
		assert(!continuation);

		continuation = _continuation;
	}

	void await_resume() const {
		assert(ready);

		if (error)
			std::rethrow_exception(error);
	}

private:
	/* virtual methods from class Lua::ResumeListener */
	void OnLuaFinished(lua_State *) noexcept override;
	void OnLuaError(lua_State *, std::exception_ptr e) noexcept override;
};

} // namespace Lua
