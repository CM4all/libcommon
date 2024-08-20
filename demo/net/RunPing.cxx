// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/net/PingClient.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/Parser.hxx"
#include "util/PrintException.hxx"

#include <stdio.h>
#include <stdlib.h>

static bool success;

class MyPingClientHandler final : public PingClientHandler {
	ShutdownListener shutdown_listener;

public:
	explicit MyPingClientHandler(EventLoop &event_loop) noexcept
		:shutdown_listener(event_loop, BIND_THIS_METHOD(OnShutdown))
	{
		shutdown_listener.Enable();
	}

	void PingResponse() noexcept override {
		success = true;
		printf("ok\n");
		shutdown_listener.Disable();
	}

	void PingError(std::exception_ptr ep) noexcept override {
		PrintException(ep);
		shutdown_listener.Disable();
	}

private:
	void OnShutdown() noexcept {
		shutdown_listener.GetEventLoop().Break();
	}
};

int
main(int argc, char **argv) noexcept
try {
	if (argc != 2) {
		fprintf(stderr, "usage: run-ping IP\n");
		return EXIT_FAILURE;
	}

	const auto address = ParseSocketAddress(argv[1], 0, false);

	EventLoop event_loop;

	MyPingClientHandler handler{event_loop};
	PingClient client(event_loop, handler);
	client.Start(address);

	event_loop.Run();

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
