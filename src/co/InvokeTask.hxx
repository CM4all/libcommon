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

#pragma once

#include "Compat.hxx"
#include "util/BindMethod.hxx"

#include <exception>
#include <utility>

namespace Co {

/**
 * A helper task which invokes a coroutine from synchronous code.
 */
class InvokeTask {
	using Callback = BoundMethod<void(std::exception_ptr error) noexcept>;

public:
	struct promise_type {
		Callback callback{nullptr};

		std::exception_ptr error;

		auto initial_suspend() noexcept {
			return std::suspend_never{};
		}

		struct final_awaitable {
			bool await_ready() const noexcept {
				return false;
			}

			template<typename PROMISE>
			void await_suspend(std::coroutine_handle<PROMISE> coro) noexcept {
				auto &p = coro.promise();
				if (p.callback)
					p.callback(std::move(p.error));
			}

			void await_resume() const noexcept {
			}
		};

		auto final_suspend() noexcept {
			return final_awaitable{};
		}

		void return_void() noexcept {
		}

		InvokeTask get_return_object() noexcept {
			return InvokeTask(std::coroutine_handle<promise_type>::from_promise(*this));
		}

		void unhandled_exception() noexcept {
			error = std::current_exception();
		}
	};

private:
	std::coroutine_handle<promise_type> coroutine;

	explicit InvokeTask(std::coroutine_handle<promise_type> _coroutine) noexcept
		:coroutine(_coroutine)
	{
	}

public:
	InvokeTask() noexcept {
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
		using std::swap;
		swap(coroutine, src.coroutine);
		return *this;
	}

	void OnCompletion(Callback callback) noexcept {
		if (coroutine.done())
			callback(std::move(coroutine.promise().error));
		else
			coroutine.promise().callback = callback;
	}
};

} // namespace Co
