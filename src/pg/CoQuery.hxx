// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "AsyncConnection.hxx"
#include "event/DeferEvent.hxx"
#include "co/Compat.hxx"

#include <cstdint>

namespace Pg {

/**
 * Asynchronous PostgreSQL query.
 *
 * Example:
 *
 *     Pg::Result result = co_await
 *       Pg::CoQuery(connection, "SELECT foo FROM bar WHERE id=$1", id);
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
	enum class CancelType : uint8_t {
		/**
		 * Using AsyncConnection::DiscardRequest().
		 */
		DISCARD,

		/**
		 * Using AsyncConnection::RequestCancel().
		 */
		CANCEL,
	};

private:
	const CancelType cancel_type;

public:
	template<typename... Params>
	CoQuery(AsyncConnection &_connection, CancelType _cancel_type,
		const Params&... params)
		:connection(_connection),
		 defer_resume(connection.GetEventLoop(),
			      BIND_THIS_METHOD(OnDeferredResume)),
		 cancel_type(_cancel_type)
	{
		// TODO are we connected?
		connection.SendQuery(*this, params...);
	}

	template<typename... Params>
	CoQuery(AsyncConnection &_connection, Params... params)
		:CoQuery(_connection, CancelType::DISCARD,
			 params...)
	{
	}

	~CoQuery() noexcept {
		if (!ready)
			Cancel();
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
				query.defer_resume.Cancel();

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
	void Cancel() noexcept {
		switch (cancel_type) {
		case CancelType::DISCARD:
			connection.DiscardRequest();
			break;

		case CancelType::CANCEL:
			connection.RequestCancel();
			break;
		}
	}

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

		if (continuation)
			defer_resume.Schedule();
	}

	void OnResultError() noexcept override {
		ready = true;
		failed = true;

		if (continuation)
			defer_resume.Schedule();
	}
};

} // namespace Co
