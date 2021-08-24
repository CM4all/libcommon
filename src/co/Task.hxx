/*
 * Copyright 2020-2021 CM4all GmbH
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

#include "UniqueHandle.hxx"
#include "Compat.hxx"

#include <cassert>
#include <exception>
#include <optional>
#include <utility>

namespace Co {

namespace detail {

template<typename R>
class promise_result_manager {
	std::optional<R> value;

public:
	template<typename U>
	void return_value(U &&_value) noexcept {
		value.emplace(std::forward<U>(_value));
	}

	decltype(auto) GetReturnValue() noexcept {
		/* this assertion can fail if control flows off the
		   end of a coroutine without co_return, which is
		   undefined behavior according to
		   https://timsong-cpp.github.io/cppwp/n4861/stmt.return.coroutine */
		assert(value);

		return std::move(*value);
	}
};

template<>
class promise_result_manager<void> {
public:
	void return_void() noexcept {}
	void GetReturnValue() noexcept {}
};

} // namespace Co::detail

/**
 * A coroutine task which is suspended initially and returns a value
 * (with support for exceptions).
 */
template<typename T>
class Task {
public:
	class promise_type : public detail::promise_result_manager<T> {
		std::coroutine_handle<> continuation;

		std::exception_ptr error;

	public:
		auto initial_suspend() noexcept {
			return std::suspend_always{};
		}

		struct final_awaitable {
			bool await_ready() const noexcept {
				return false;
			}

			template<typename PROMISE>
			std::coroutine_handle<> await_suspend(std::coroutine_handle<PROMISE> coro) noexcept {
				return coro.promise().continuation;
			}

			void await_resume() noexcept {
			}
		};

		auto final_suspend() noexcept {
			return final_awaitable{};
		}

		Task<T> get_return_object() noexcept {
			return Task<T>(std::coroutine_handle<promise_type>::from_promise(*this));
		}

		void unhandled_exception() noexcept {
			error = std::current_exception();
		}

		void SetContinuation(std::coroutine_handle<> _continuation) noexcept {
			continuation = _continuation;
		}

		decltype(auto) GetReturnValue() {
			if (error)
				std::rethrow_exception(std::move(error));

			return detail::promise_result_manager<T>::GetReturnValue();
		}
	};

private:
	UniqueHandle<promise_type> coroutine;

	explicit Task(std::coroutine_handle<promise_type> _coroutine) noexcept
		:coroutine(_coroutine)
	{
	}

public:
	Task() = default;

	auto operator co_await() const noexcept {
		struct Awaitable final {
			const std::coroutine_handle<promise_type> coroutine;

			bool await_ready() const noexcept {
				return coroutine.done();
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
				coroutine.promise().SetContinuation(continuation);
				return coroutine;
			}

			decltype(auto) await_resume() {
				return coroutine.promise().GetReturnValue();
			}
		};

		return Awaitable{coroutine.get()};
	}
};

} // namespace Co
