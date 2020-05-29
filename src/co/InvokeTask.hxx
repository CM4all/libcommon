/*
 * Copyright 2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <coroutine>
#include <exception>
#include <utility>

#ifndef __cpp_impl_coroutine
#error Need -fcoroutines
#endif

namespace Co {

/**
 * A helper task which invokes a coroutine from synchronous code.
 */
struct InvokeTask {
	struct promise_type {
		auto initial_suspend() noexcept {
			return std::suspend_never{};
		}

		auto final_suspend() noexcept {
			return std::suspend_always{};
		}

		void return_void() noexcept {
		}

		InvokeTask get_return_object() noexcept {
			return InvokeTask(std::coroutine_handle<promise_type>::from_promise(*this));
		}

		void unhandled_exception() noexcept {
			std::terminate();
		}
	};

	std::coroutine_handle<promise_type> coroutine;

	InvokeTask() noexcept {
	}

	explicit InvokeTask(std::coroutine_handle<promise_type> _coroutine) noexcept
		:coroutine(_coroutine)
	{
	}

	InvokeTask(InvokeTask &&src) noexcept
		:coroutine(std::exchange(src.coroutine, nullptr))
	{
	}

	~InvokeTask() noexcept {
		if (coroutine)
			coroutine.destroy();
	}

	InvokeTask &operator=(InvokeTask &&src) noexcept {
		coroutine = std::exchange(src.coroutine, nullptr);
		return *this;
	}
};

} // namespace Co
