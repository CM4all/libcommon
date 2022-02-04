/*
 * Copyright 2020-2021 CM4all GmbH
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

#include "lib/curl/Global.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/IntrusiveList.hxx"
#include "util/PrintException.hxx"

#include <cstdio>
#include <cstdlib>
#include <memory>

#include <fcntl.h>

struct Instance;

class Request final : public IntrusiveListHook, CurlResponseHandler {
	Instance &instance;

	CurlRequest r;

public:
	Request(Instance &_instance, const char *url) noexcept;

	void Start() {
		r.Start();
	}

private:
	/* virtual methods from CurlResponseHandler */
	void OnHeaders(unsigned status,
		       Curl::Headers &&headers) override {
		fprintf(stderr, "status %u\n", status);

		for (const auto &i : headers)
			fprintf(stderr, "%s: %s\n",
				i.first.c_str(), i.second.c_str());

		fprintf(stderr, "\n");
	}

	void OnData(ConstBuffer<void> data) override {
		write(STDOUT_FILENO, data.data, data.size);
	}

	virtual void OnEnd() override;
	virtual void OnError(std::exception_ptr e) noexcept override;
};

struct Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	CurlGlobal curl_global{event_loop};

	IntrusiveList<Request> requests;

	std::exception_ptr error;

	Instance() noexcept
	{
		shutdown_listener.Enable();
	}

	void CancelAllRequests() noexcept {
		requests.clear_and_dispose(DeleteDisposer{});
	}

	void OnShutdown() noexcept {
		CancelAllRequests();
	}

	void AddRequest(const char *url) {
		auto *r = new Request(*this, url);
		requests.push_front(*r);
		r->Start();
	}

	void RemoveRequest(Request &r) noexcept {
		r.unlink();
		if (requests.empty())
			shutdown_listener.Disable();
	}

	void Fail(std::exception_ptr e) noexcept {
		CancelAllRequests();
		shutdown_listener.Disable();
		error = std::move(e);
	}
};

Request::Request(Instance &_instance, const char *url) noexcept
	:instance(_instance),
	 r(instance.curl_global, url, *this)
{
}

void
Request::OnEnd()
{
	instance.RemoveRequest(*this);
}

void
Request::OnError(std::exception_ptr e) noexcept
{
	instance.error = std::move(e);
	instance.RemoveRequest(*this);
}

int
main(int argc, char **argv) noexcept
try {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s URL...\n", argv[0]);
		return EXIT_FAILURE;
	}

	Instance instance;

	for (int i = 1; i < argc; ++i)
		instance.AddRequest(argv[i]);

	instance.event_loop.Dispatch();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
