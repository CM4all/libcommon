// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "InvokeTask.hxx"

#include <cassert>
#include <exception>
#include <utility>

namespace Co {

/**
 * A helper class which wraps #InvokeTask to make invoking a coroutine
 * from synchronous code even simpler.  It implements a completion
 * callback (but does not have any way to signal to this object's
 * user) and stores the error condition.  It is mostly useful for
 * integration in unit tests.
 *
 * To use it, pass the task to Start().  Then let the coroutine
 * execute (e.g. using an #EventLoop), and when it is done, call
 * Finish() to rethrow the exception.
 */
class Runner {
	Co::InvokeTask task;

	std::exception_ptr error;

public:
	bool IsDone() const noexcept {
		return !task;
	}

	void Start(Co::InvokeTask &&_task) noexcept {
		assert(!task);
		assert(!error);

		task = std::move(_task);
		task.Start(BIND_THIS_METHOD(OnCompletion));
	}

	void Finish() {
		assert(IsDone());

		if (error)
			std::rethrow_exception(std::move(error));
	}

	void Cancel() noexcept {
		task = {};
	}

private:
	void OnCompletion(std::exception_ptr &&_error) noexcept {
		assert(!task);
		assert(!error);

		error = std::move(_error);
	}
};

} // namespace Co
