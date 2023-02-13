// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

class Request final
	: public IntrusiveListHook<IntrusiveHookMode::NORMAL>,
	  CurlResponseHandler
{
	Instance &instance;

	CurlRequest r;

public:
	Request(Instance &_instance, const char *url) noexcept;

	void Start() {
		r.Start();
	}

private:
	/* virtual methods from CurlResponseHandler */
	void OnHeaders(HttpStatus status,
		       Curl::Headers &&headers) override {
		fprintf(stderr, "status %u\n", static_cast<unsigned>(status));

		for (const auto &i : headers)
			fprintf(stderr, "%s: %s\n",
				i.first.c_str(), i.second.c_str());

		fprintf(stderr, "\n");
	}

	void OnData(std::span<const std::byte> data) override {
		write(STDOUT_FILENO, data.data(), data.size());
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

	instance.event_loop.Run();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
