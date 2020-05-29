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

#include "CoHandler.hxx"
#include "Response.hxx"
#include "Connection.hxx"
#include "co/Task.hxx"
#include "co/InvokeTask.hxx"
#include "util/Cancellable.hxx"

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

public:
	CoRequest(Connection &_connection, Co::Task<Response> &&_task) noexcept
		:connection(_connection),
		 task(std::move(_task)) {}

	void Start(CancellablePointer &cancel_ptr) noexcept {
		cancel_ptr = *this;
		dummy_task = Handle();
	}

private:
	Co::InvokeTask Handle() noexcept {
		try {
			connection.SendResponse(co_await std::move(task));
		} catch (...) {
			Response response;
			response.Status(HTTP_STATUS_INTERNAL_SERVER_ERROR);
			connection.SendResponse(std::move(response));
		}

		delete this;
	}

	/* virtual methods from class Cancellable */
	void Cancel() noexcept override {
		delete this;
	}
};

}

void
CoHandler::OnTranslationRequest(Connection &connection,
				const Request &request,
				CancellablePointer &cancel_ptr) noexcept
{
	auto *r = new CoRequest(connection,
				OnTranslationRequest(request));
	r->Start(cancel_ptr);
}

} // namespace Translation::Server
