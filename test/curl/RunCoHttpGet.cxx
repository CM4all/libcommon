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

	printf("status=%u\n", response.status);
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

	instance.event_loop.Dispatch();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
