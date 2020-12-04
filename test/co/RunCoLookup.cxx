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
	ShutdownListener shutdown_listener;

	Cares::Channel channel{event_loop};

	Co::InvokeTask task;

	std::exception_ptr error;

	Instance()
		:shutdown_listener(event_loop, BIND_THIS_METHOD(OnShutdown))
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
		if (ToString(buffer, sizeof(buffer), i))
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

	instance.event_loop.Dispatch();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
