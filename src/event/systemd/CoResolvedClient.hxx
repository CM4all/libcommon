// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "ResolvedClient.hxx"
#include "co/AwaitableHelper.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "util/Cancellable.hxx"

#include <vector>

namespace Systemd {

class CoResolveHostname final : ResolveHostnameHandler {
	std::coroutine_handle<> continuation;

	std::vector<AllocatedSocketAddress> value;

	std::exception_ptr error;

	CancellablePointer cancel_ptr{nullptr};

	using Awaitable = Co::AwaitableHelper<CoResolveHostname>;
	friend Awaitable;

public:
	CoResolveHostname(EventLoop &event_loop,
			  std::string_view hostname, unsigned port=0,
			  int family=AF_UNSPEC) noexcept {
		ResolveHostname(event_loop, hostname, port, family,
				*this, cancel_ptr);
	}

	~CoResolveHostname() noexcept {
		if (cancel_ptr)
			cancel_ptr.Cancel();
	}

	Awaitable operator co_await() noexcept {
		return *this;
	}

private:
	bool IsReady() const noexcept {
		return !cancel_ptr;
	}

	std::vector<AllocatedSocketAddress> TakeValue() noexcept {
		return std::move(value);
	}

	/* virtual methods from ResolveHostnameHandler */
	void OnResolveHostname(std::span<const SocketAddress> addresses) noexcept override {
		cancel_ptr = nullptr;

		value.reserve(addresses.size());
		for (const SocketAddress i : addresses)
			value.emplace_back(i);

		if (continuation)
			continuation.resume();
	}

	void OnResolveHostnameError(std::exception_ptr _error) noexcept override {
		cancel_ptr = nullptr;
		error = std::move(_error);

		if (continuation)
			continuation.resume();
	}
};

} // namespace Systemd
