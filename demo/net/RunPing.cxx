// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/net/PingClient.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/Parser.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

#include <stdlib.h>

static bool success;

class Instance final : PingClientHandler {
	EventLoop event_loop;

	ShutdownListener shutdown_listener;

	PingClient client{event_loop, *this};

public:
	Instance() noexcept
		:shutdown_listener(event_loop, BIND_THIS_METHOD(OnShutdown))
	{
		shutdown_listener.Enable();
	}

	void Start(SocketAddress address) noexcept {
		client.Start(address);
	}

	void Run() noexcept {
		event_loop.Run();
	}

private:
	void OnShutdown() noexcept {
		client.Cancel();
	}

	// virtual methods from class PingClientHandler
	void PingResponse() noexcept override {
		success = true;
		fmt::print("ok\n");
		shutdown_listener.Disable();
	}

	void PingError(std::exception_ptr ep) noexcept override {
		PrintException(ep);
		shutdown_listener.Disable();
	}
};

int
main(int argc, char **argv) noexcept
try {
	if (argc != 2) {
		fmt::print("usage: {} IP\n", argv[0]);
		return EXIT_FAILURE;
	}

	const auto address = ParseSocketAddress(argv[1], 0, false);

	Instance instance;
	instance.Start(address);
	instance.Run();

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
