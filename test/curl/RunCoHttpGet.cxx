// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/curl/Global.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/CoRequest.hxx"
#include "co/InvokeTask.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "util/PrintException.hxx"

#include <cstdio>
#include <cstdlib>

struct Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	CurlGlobal curl_global{event_loop};

	Co::InvokeTask task;

	std::exception_ptr error;

	Instance() noexcept
	{
		shutdown_listener.Enable();
	}

	void OnShutdown() noexcept {
		task = {};
	}

	void OnCompletion(std::exception_ptr _error) noexcept {
		error = std::move(_error);
		shutdown_listener.Disable();
	}
};

static Co::InvokeTask
Run(CurlGlobal &global, const char *url)
{
	const auto response = co_await Curl::CoRequest(global, CurlEasy(url));

	printf("status=%u\n", static_cast<unsigned>(response.status));
	for (const auto &[key, value] : response.headers)
		printf("%s: %s\n", key.c_str(), value.c_str());
	printf("\n");
	fputs(response.body.c_str(), stdout);
}

int
main(int argc, char **argv) noexcept
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s URL\n", argv[0]);
		return EXIT_FAILURE;
	}

	Instance instance;
	instance.task = Run(instance.curl_global, argv[1]);
	instance.task.Start(BIND_METHOD(instance, &Instance::OnCompletion));

	instance.event_loop.Run();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
