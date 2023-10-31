// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "event/systemd/CoResolvedClient.hxx"
#include "co/InvokeTask.hxx"
#include "net/FormatAddress.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "util/PrintException.hxx"

#include <cstdio>
#include <cstdlib>

struct Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	Co::InvokeTask task;

	std::exception_ptr error;

	Instance()
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

static void
PrintResult(const std::vector<AllocatedSocketAddress> &result) noexcept
{
	for (const auto &i : result) {
		char buffer[256];
		if (ToString(std::span{buffer}, i))
			printf("%s\n", buffer);
	}
}

static Co::InvokeTask
Run(EventLoop &event_loop, const char *name)
{
	const auto result = co_await Systemd::CoResolveHostname(event_loop, name);
	PrintResult(result);
}

int
main(int argc, char **argv) noexcept
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s NAME\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *const name = argv[1];

	Instance instance;

	instance.task = Run(instance.event_loop, name);
	instance.task.Start(BIND_METHOD(instance, &Instance::OnCompletion));

	instance.event_loop.Run();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
