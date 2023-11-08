// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "co/Compat.hxx"

class CoConnectSocket final : ConnectSocketHandler {
	ConnectSocket connect;

	std::coroutine_handle<> continuation;

	UniqueSocketDescriptor value;

	std::exception_ptr error;

public:
	CoConnectSocket(EventLoop &event_loop,
			const auto &address,
			Event::Duration timeout) noexcept
		:connect(event_loop, *this)
	{
		connect.Connect(address, timeout);
	}

	auto operator co_await() noexcept {
		struct Awaitable final {
			CoConnectSocket &task;

			bool await_ready() const noexcept {
				return !task.connect.IsPending();
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) noexcept {
				task.continuation = _continuation;
				return std::noop_coroutine();
			}

			decltype(auto) await_resume() {
				if (task.error)
					std::rethrow_exception(task.error);

				return std::move(task.value);
			}
		};

		return Awaitable{*this};
	}

private:
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
