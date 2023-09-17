// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "co/InvokeTask.hxx"
#include "net/ToString.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/net/cares/Channel.hxx"
#include "event/net/cares/CoLookup.hxx"
#include "util/PrintException.hxx"

#include <cstdio>
#include <cstdlib>

struct Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	Cares::Channel channel{event_loop};

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
Run(Cares::Channel &channel, const char *name)
{
	const auto result = co_await Cares::CoLookup(channel, name);
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

	instance.task = Run(instance.channel, name);
	instance.task.Start(BIND_METHOD(instance, &Instance::OnCompletion));

	instance.event_loop.Run();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
