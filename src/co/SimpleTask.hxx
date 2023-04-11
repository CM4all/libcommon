// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "UniqueHandle.hxx"

#include <exception> // for std::terminate()

namespace Co {

/**
 * A simple task that returns void and cannot throw.  It is initially
 * suspended and cannot be awaited.  This class only exists as an easy
 * way to create a std::coroutine_handle.
 */
class SimpleTask final {
public:
	struct promise_type final {
		[[nodiscard]]
		auto initial_suspend() noexcept {
			return std::suspend_always{};
		}

		void return_void() noexcept {
		}

		[[nodiscard]]
		auto final_suspend() noexcept {
			return std::suspend_always{};
		}

		[[nodiscard]]
		auto get_return_object() noexcept {
			return SimpleTask(std::coroutine_handle<promise_type>::from_promise(*this));
		}

		void unhandled_exception() noexcept {
			std::terminate();
		}
	};

private:
	UniqueHandle<promise_type> coroutine;

	[[nodiscard]]
	explicit SimpleTask(std::coroutine_handle<promise_type> _coroutine) noexcept
		:coroutine(_coroutine)
	{
	}

public:
	[[nodiscard]]
	SimpleTask() = default;

	bool IsDefined() const noexcept {
		return coroutine;
	}

	[[nodiscard]]
	operator std::coroutine_handle<>() const noexcept {
		return coroutine.get();
	}

	[[nodiscard]]
	operator UniqueHandle<>() && noexcept {
		return std::move(coroutine);
	}
};

} // namespace Co
