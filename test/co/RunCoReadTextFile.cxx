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

#include "co/InvokeTask.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/uring/Manager.hxx"
#include "io/uring/CoTextFile.hxx"
#include "io/uring/CoOperation.hxx"
#include "util/PrintException.hxx"

#include <cstdio>
#include <cstdlib>
#include <memory>

#include <fcntl.h>

struct Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener;

	Uring::Manager uring;

	Co::InvokeTask task;

	std::exception_ptr error;

	Instance()
		:shutdown_listener(event_loop, BIND_THIS_METHOD(OnShutdown)),
		 uring(event_loop)
	{
		shutdown_listener.Enable();
	}

	void OnShutdown() noexcept {
		task = {};
		uring.SetVolatile();
	}

	void OnCompletion(std::exception_ptr _error) noexcept {
		error = std::move(_error);
		uring.SetVolatile();
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
			 result.data(), result.size(), 0);
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

	instance.task = Run(instance.uring, path);
	instance.task.Start(BIND_METHOD(instance, &Instance::OnCompletion));

	instance.event_loop.Dispatch();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
