// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "co/AwaitableHelper.hxx"

class CoConnectSocket final : ConnectSocketHandler {
	ConnectSocket connect;

	std::coroutine_handle<> continuation;

	UniqueSocketDescriptor value;

	std::exception_ptr error;

	using Awaitable = Co::AwaitableHelper<CoConnectSocket>;
	friend Awaitable;

public:
	CoConnectSocket(EventLoop &event_loop,
			const auto &address,
			Event::Duration timeout) noexcept
		:connect(event_loop, *this)
	{
		connect.Connect(address, timeout);
	}

	Awaitable operator co_await() noexcept {
		return *this;
	}

private:
	bool IsReady() const noexcept {
		return !connect.IsPending();
	}

	UniqueSocketDescriptor TakeValue() noexcept {
		return std::move(value);
	}

	/* virtual methods from ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override {
		value = std::move(fd);

		if (continuation)
			continuation.resume();
	}

	void OnSocketConnectError(std::exception_ptr _error) noexcept override {
		error = std::move(_error);

		if (continuation)
			continuation.resume();
	}
};
