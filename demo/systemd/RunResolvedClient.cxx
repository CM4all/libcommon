// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/fmt/SocketAddressFormatter.hxx"
#include "event/systemd/ResolvedClient.hxx"
#include "event/Loop.hxx"
#include "util/Cancellable.hxx"
#include "util/PrintException.hxx"

#include <fmt/format.h>

int
main(int argc, char **argv) noexcept
try {
	if (argc != 2) {
		fmt::print(stderr, "Usage: {} HOSTNAME\n", argv[0]);
		return EXIT_FAILURE;
	}

	EventLoop event_loop;

	struct Handler final : Systemd::ResolveHostnameHandler {
		std::exception_ptr error;

		/* virtual methods from ResolveHostnameHandler */
		void OnResolveHostname(std::span<const SocketAddress> addresses) noexcept override {
			for (const SocketAddress i : addresses)
				fmt::print("{}\n", i);
		}

		void OnResolveHostnameError(std::exception_ptr _error) noexcept override {
			error = std::move(_error);
		}
	};

	Handler handler;
	CancellablePointer cancel_ptr;

	Systemd::ResolveHostname(event_loop, argv[1], 3306, handler, cancel_ptr);
	event_loop.Run();

	if (handler.error)
		std::rethrow_exception(handler.error);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
