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

#include <exception>
#include <optional>
#include <utility>

namespace Co {

namespace detail {

template<typename R>
struct promise_result_manager {
	std::optional<R> value;

	template<typename U>
	void return_value(U &&_value) noexcept {
		value.emplace(std::forward<U>(_value));
	}

	decltype(auto) GetReturnValue() noexcept {
		return std::move(*value);
	}
};

template<>
struct promise_result_manager<void> {
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
	struct promise_type : detail::promise_result_manager<T> {
		std::coroutine_handle<> continuation;

		std::exception_ptr error;

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

		decltype(auto) GetReturnValue() {
			if (error)
				std::rethrow_exception(std::move(error));

			return detail::promise_result_manager<T>::GetReturnValue();
		}
	};

private:
	std::coroutine_handle<promise_type> coroutine;

	explicit Task(std::coroutine_handle<promise_type> _coroutine) noexcept
		:coroutine(_coroutine)
	{
	}

public:
	Task() = default;

	Task(Task &&src) noexcept
		:coroutine(std::exchange(src.coroutine, nullptr))
	{
	}

	~Task() noexcept {
		if (coroutine)
			coroutine.destroy();
	}

	Task &operator=(Task &&src) noexcept {
		using std::swap;
		swap(coroutine, src.coroutine);
		return *this;
	}

	auto operator co_await() const noexcept {
		struct Awaitable final {
			const std::coroutine_handle<promise_type> coroutine;

			bool await_ready() const noexcept {
				return coroutine.done();
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
				coroutine.promise().continuation = continuation;
				return coroutine;
			}

			decltype(auto) await_resume() {
				return coroutine.promise().GetReturnValue();
			}
		};

		return Awaitable{coroutine};
	}
};

} // namespace Co
