// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "event/net/cares/Channel.hxx"
#include "event/net/cares/Handler.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "net/SocketAddress.hxx"
#include "net/ToString.hxx"
#include "util/Cancellable.hxx"
#include "util/PrintException.hxx"

#include <stdio.h>

class MyHandler final : public Cares::Handler {
	EventLoop &event_loop;

	bool done = false;

public:
	explicit MyHandler(EventLoop &_event_loop) noexcept
		:event_loop(_event_loop) {}

	bool IsDone() const noexcept {
		return done;
	}

	/* virtual methods from Cares::Handler */
	void OnCaresAddress(SocketAddress address) noexcept override {
		char buffer[256];
		if (ToString(buffer, sizeof(buffer), address))
			printf("%s\n", buffer);
	}

	void OnCaresSuccess() noexcept override {
		event_loop.Break();
		done = true;
	}

	void OnCaresError(std::exception_ptr e) noexcept override {
		PrintException(e);
		event_loop.Break();
		done = true;
	}
};

class ShutdownCancel {
	ShutdownListener shutdown_listener;
	CancellablePointer cancel_ptr;

public:
	explicit ShutdownCancel(EventLoop &event_loop)
		:shutdown_listener(event_loop,
				   BIND_THIS_METHOD(ShutdownCallback)) {
		shutdown_listener.Enable();
	}

	operator CancellablePointer &() noexcept {
		return cancel_ptr;
	}

private:
	void ShutdownCallback() noexcept {
		if (cancel_ptr) {
			cancel_ptr.Cancel();
			cancel_ptr = nullptr;
		}
	}
};

int
main(int argc, char **argv)
try {
	if (argc != 2)
		throw "Usage: RunCares HOSTNAME";

	const char *hostname = argv[1];

	EventLoop event_loop;
	ShutdownCancel shutdown_cancel(event_loop);

	Cares::Channel channel(event_loop);

	MyHandler handler(event_loop);
	CancellablePointer cancel_ptr;
	channel.Lookup(hostname, handler, cancel_ptr);

	if (!handler.IsDone())
		event_loop.Run();

	return EXIT_SUCCESS;
} catch (const char *msg) {
	fprintf(stderr, "%s\n", msg);
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
