// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CoHandler.hxx"
#include "Response.hxx"
#include "Connection.hxx"
#include "co/Task.hxx"
#include "co/InvokeTask.hxx"
#include "util/Cancellable.hxx"

#include <cassert>
#include <memory>

namespace Translation::Server {

namespace {

class CoRequest final : Cancellable {
	Connection &connection;

	/**
	 * The CoHandler::OnTranslationRequest() virtual method
	 * (coroutine) call.
	 */
	Co::Task<Response> task;

	/**
         * Our Handle() coroutine call.
	 */
	Co::InvokeTask dummy_task;

	bool result = true, starting = true, complete = false;

public:
	CoRequest(Connection &_connection, Co::Task<Response> &&_task) noexcept
		:connection(_connection),
		 task(std::move(_task)) {}

	bool Start(CancellablePointer &cancel_ptr) noexcept {
		assert(starting);
		assert(!complete);

		cancel_ptr = *this;
		dummy_task = Handle();
		dummy_task.Start(BIND_THIS_METHOD(OnCompletion));

		assert(starting);

		starting = false;
		const bool _result = result;
		if (complete)
			delete this;

		return _result;
	}

private:
	void OnCompletion(std::exception_ptr) noexcept {
		assert(!complete);

		if (starting)
			/* postpone destruction to allow Start() to
			   read the result */
			complete = true;
		else
			delete this;
	}

	Co::InvokeTask Handle() noexcept {
		try {
			result = connection.SendResponse(co_await std::move(task));
		} catch (...) {
			Response response;
			response.Status(HttpStatus::INTERNAL_SERVER_ERROR);
			result = connection.SendResponse(std::move(response));
		}
	}

	/* virtual methods from class Cancellable */
	void Cancel() noexcept override {
		assert(!complete);

		if (starting)
			/* postpone destruction to allow Start() to
			   read the result */
			complete = true;
		else
			delete this;
	}
};

}

bool
CoHandler::OnTranslationRequest(Connection &connection,
				const Request &request,
				CancellablePointer &cancel_ptr) noexcept
{
	auto *r = new CoRequest(connection,
				OnTranslationRequest(request));
	return r->Start(cancel_ptr);
}

} // namespace Translation::Server
