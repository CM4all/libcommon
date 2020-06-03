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

#include "AsyncConnection.hxx"
#include "event/DeferEvent.hxx"
#include "co/Compat.hxx"

namespace Pg {

/**
 * Asynchronous PostgreSQL query.
 */
class CoQuery final : public AsyncResultHandler {
	AsyncConnection &connection;

	/**
	 * This moves resuming the coroutine onto a new stack frame,
	 * out of the #AsyncResultHandler method calls.  Inside those,
	 * it can be unsafe to use the #AsyncConnection.
	 */
	DeferEvent defer_resume;

	Result result;

	std::coroutine_handle<> continuation;

	bool ready = false, failed = false;

public:
	template<typename... Params>
	CoQuery(AsyncConnection &_connection, Params... params) noexcept
		:connection(_connection),
		 defer_resume(connection.GetEventLoop(),
			      BIND_THIS_METHOD(OnDeferredResume))
	{
		// TODO are we connected?
		connection.SendQuery(*this, params...);
	}

	~CoQuery() noexcept {
		if (!ready)
			connection.RequestCancel();
	}

	auto operator co_await() noexcept {
		struct Awaitable final {
			CoQuery &query;

			bool await_ready() const noexcept {
				return query.ready;
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) const noexcept {
				query.continuation = _continuation;
				return std::noop_coroutine();
			}

			Result await_resume() const {
				if (query.failed)
					throw std::runtime_error("Database connection failed");

				if (query.result.IsError())
					throw Error(std::move(query.result));

				return std::move(query.result);
			}
		};

		return Awaitable{*this};
	}

private:
	void OnDeferredResume() noexcept {
		continuation.resume();
	}

	/* virtual methods from Pg::AsyncResultHandler */
	void OnResult(Pg::Result &&_result) override {
		result = std::move(_result);
		// TODO: yield for each row?
	}

	void OnResultEnd() override {
		ready = true;
		defer_resume.Schedule();
	}

	void OnResultError() noexcept override {
		ready = true;
		failed = true;
		defer_resume.Schedule();
	}
};

} // namespace Co
