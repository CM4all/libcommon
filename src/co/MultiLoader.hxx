// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "MultiAwaitable.hxx"
#include "Task.hxx"

#include <cassert>
#include <exception>
#include <variant>

namespace Co {

/**
 * A class that helps with loading a value in a coroutine.  It
 * supports multiple waiters.
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
