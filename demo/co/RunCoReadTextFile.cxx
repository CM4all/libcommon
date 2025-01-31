// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "co/InvokeTask.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "io/uring/Queue.hxx"
#include "io/uring/CoTextFile.hxx"
#include "io/uring/CoOperation.hxx"
#include "util/PrintException.hxx"
#include "util/SpanCast.hxx"

#include <cstdio>
#include <cstdlib>
#include <memory>

#include <fcntl.h>

using Uring::CoWrite;

struct Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener;

	Co::InvokeTask task;

	std::exception_ptr error;

	Instance()
		:shutdown_listener(event_loop, BIND_THIS_METHOD(OnShutdown))
	{
		event_loop.EnableUring(1024, IORING_SETUP_SINGLE_ISSUER|IORING_SETUP_COOP_TASKRUN);
		shutdown_listener.Enable();
	}

	void OnShutdown() noexcept {
		task = {};
		event_loop.SetVolatile();
	}

	void OnCompletion(std::exception_ptr _error) noexcept {
		error = std::move(_error);
		event_loop.SetVolatile();
		shutdown_listener.Disable();
	}
};

static Co::InvokeTask
Run(Uring::Queue &queue, const char *path)
{
	auto result = co_await Uring::CoReadTextFile(queue,
						     FileDescriptor(AT_FDCWD),
						     path);
	co_await CoWrite(queue, FileDescriptor(STDOUT_FILENO),
			 AsBytes(result),
			 0);
}

int
main(int argc, char **argv) noexcept
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s PATH\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *const path = argv[1];

	Instance instance;

	instance.task = Run(*instance.event_loop.GetUring(), path);
	instance.task.Start(BIND_METHOD(instance, &Instance::OnCompletion));

	instance.event_loop.Run();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
