/*
 * Copyright 2007-2022 CM4all GmbH
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
#include "SimpleServer.hxx"
#include "was/ExceptionResponse.hxx"
#include "co/InvokeTask.hxx"
#include "co/Task.hxx"
#include "util/Cancellable.hxx"

namespace Was {

class CoSimpleRequestHandler::Request final : Cancellable {
	SimpleServer &server;

	Co::Task<SimpleResponse> task;

	Co::InvokeTask dummy_task;

	bool result = true, starting = true, complete = false;

public:
	Request(SimpleServer &_server, Co::Task<SimpleResponse> &&_task) noexcept
		:server(_server),
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
			result = server.SendResponse(co_await std::move(task));
		} catch (const Was::NotFound &e) {
			SimpleResponse response;
			response.status = HTTP_STATUS_NOT_FOUND;
			response.SetTextPlain(e.body);
			result = server.SendResponse(std::move(response));
		} catch (const Was::BadRequest &e) {
			SimpleResponse response;
			response.status = HTTP_STATUS_BAD_REQUEST;
			response.SetTextPlain(e.body);
			result = server.SendResponse(std::move(response));
		} catch (...) {
			SimpleResponse response;
			response.status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
			result = server.SendResponse(std::move(response));
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

bool
CoSimpleRequestHandler::OnRequest(SimpleServer &server,
				  SimpleRequest &&request,
				  CancellablePointer &cancel_ptr) noexcept
{
	try {
		auto *r = new Request(server,
				      OnCoRequest(std::move(request)));
		return r->Start(cancel_ptr);
	} catch (const Was::NotFound &e) {
		SimpleResponse response;
		response.status = HTTP_STATUS_NOT_FOUND;
		response.SetTextPlain(e.body);
		return server.SendResponse(std::move(response));
	} catch (const Was::BadRequest &e) {
		SimpleResponse response;
		response.status = HTTP_STATUS_BAD_REQUEST;
		response.SetTextPlain(e.body);
		return server.SendResponse(std::move(response));
	}
}

} // namespace Was
