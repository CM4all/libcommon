// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef BENG_PROXY_SHUTDOWN_LISTENER_HXX
#define BENG_PROXY_SHUTDOWN_LISTENER_HXX

#include "SignalEvent.hxx"

/**
 * Listener for shutdown signals (SIGTERM, SIGINT, SIGQUIT).
 */
class ShutdownListener {
	SignalEvent event;

	using Callback = BoundMethod<void() noexcept>;
	const Callback callback;

public:
	ShutdownListener(EventLoop &loop, Callback _callback) noexcept;

	ShutdownListener(const ShutdownListener &) = delete;
	ShutdownListener &operator=(const ShutdownListener &) = delete;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Enable() {
		event.Enable();
	}

	void Disable() noexcept {
		event.Disable();
	}

private:
	void SignalCallback(int signo) noexcept;
};

#endif
