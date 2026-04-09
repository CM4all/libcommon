// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lib/curl/Global.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/CoRequest.hxx"
#include "co/InvokeTask.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"

#include <fmt/core.h>

#include <cstdlib>

using std::string_view_literals::operator""sv;

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

	void OnCompletion(std::exception_ptr &&_error) noexcept {
		error = std::move(_error);
		shutdown_listener.Disable();
	}
};

static Co::InvokeTask
Run(CurlGlobal &global, const char *url, Curl::CoOptions options)
{
	const auto response = co_await Curl::CoRequest(global, CurlEasy{url}, options);

	fmt::print(stderr, "status={}\n"sv, std::to_underlying(response.status));

	for (const auto &i : response.headers)
		fmt::print(stderr, "{}: {}\n"sv, i.first, i.second);

	fmt::print(stderr, "\n"sv);
	fflush(stderr);

	(void)write(STDOUT_FILENO, response.body.data(), response.body.size());
}

int
main(int argc, char **argv) noexcept
try {
	Curl::CoOptions options;

	int pos;
	for (pos = 1; pos < argc; ++pos) {
		if (const char *max_size = StringAfterPrefix(argv[pos], "--max-size="sv)) {
			options.max_size = strtoull(max_size, nullptr, 10);
		} else
			break;
	}

	if (pos + 1 != argc) {
		fmt::print(stderr, "Usage: %s [--max-size=BYTES] URL\n"sv, argv[0]);
		return EXIT_FAILURE;
	}

	const char *const url = argv[pos];

	Instance instance;
	instance.task = Run(instance.curl_global, url, options);
	instance.task.Start(BIND_METHOD(instance, &Instance::OnCompletion));

	instance.event_loop.Run();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
