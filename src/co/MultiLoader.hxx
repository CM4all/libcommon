// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "MultiAwaitable.hxx"
#include "Task.hxx"

#include <cassert>
#include <exception>
#include <variant>

namespace Co {

/**
 * A class that helps with loading a value in a coroutine.  It
 * supports multiple waiters.  To use it, call get().
 */
template<typename T>
class MultiLoader {
	/**
	 * The coroutine task that loads the value.
	 */
	MultiAwaitable task;

	/**
	 * The actual value (or an exception if an error has
	 * occurred).
	 */
	std::variant<std::monostate, T, std::exception_ptr> value;

public:
	/**
	 * Create a new task that can be used to await the value.
	 *
	 * @param f a function that loads the actual value; it will be
	 * called at most once and its return value will be stored in
	 * this #MultiLoader instance for all (current and future)
	 * waiters
	 */
	[[nodiscard]]
	Co::Task<const T &> get(std::invocable<> auto f) {
		while (true) {
			assert(!value.valueless_by_exception());

			/* do we have the value already? */
			if (const auto *v = std::get_if<T>(&value))
				co_return *v;

			/* or do we have an error? */
			if (const auto *e = std::get_if<std::exception_ptr>(&value))
				std::rethrow_exception(*e);

			/* both no - invoke the loader function and await
			   completion */
			if (!task.IsActive())
				task.Start(Load(f));

			co_await task;

			/* by now, either a value or an error is set,
			   so we repeat the above check */
			assert(!std::holds_alternative<std::monostate>(value));
		}
	}

	/**
	 * Artificially inject a value, marking this loader "ready".
	 * All future get() calls will complete immediately and will
	 * return this value; the specified loader function will be
	 * ignored.  This is only allowed if get() has never been
	 * called.
	 */
	template<typename... Args>
	void InjectValue(Args&&... args) noexcept {
		assert(std::holds_alternative<std::monostate>(value));
		assert(!task.IsActive());

		value.template emplace<T>(std::forward<Args>(args)...);
	}

	/**
	 * Like InjectValue(), but store an error.  All future get()
	 * calls will rethrow this exception.
	 */
	void InjectError(std::exception_ptr &&error) noexcept {
		assert(std::holds_alternative<std::monostate>(value));
		assert(!task.IsActive());

		value.template emplace<std::exception_ptr>(std::move(error));
	}

private:
	[[nodiscard]]
	Co::EagerTask<void> Load(std::invocable<> auto f) try {
		assert(std::holds_alternative<std::monostate>(value));

		value = co_await f();
	} catch (...) {
		value = std::current_exception();
	}
};

} // namespace Co
