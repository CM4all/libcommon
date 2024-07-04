// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Interface.hxx"
#include "co/AwaitableHelper.hxx"
#include "util/Cancellable.hxx"

/**
 * Coroutine adapter for SpawnService::Enqueue().  The awaiting
 * coroutine is resumed as soon as the spawner is ready.
 */
class CoEnqueueSpawner {
	std::coroutine_handle<> continuation;

	using Awaitable = Co::AwaitableHelper<CoEnqueueSpawner, false>;
	friend Awaitable;

	CancellablePointer cancel_ptr;

public:
	explicit CoEnqueueSpawner(SpawnService &spawn_service) noexcept {
		spawn_service.Enqueue(BIND_THIS_METHOD(OnReady), cancel_ptr);
	}

	~CoEnqueueSpawner() noexcept {
		if (cancel_ptr)
			cancel_ptr.Cancel();
	}

	CoEnqueueSpawner(const CoEnqueueSpawner &) = delete;
	CoEnqueueSpawner &operator=(const CoEnqueueSpawner &) = delete;

	[[nodiscard]]
	Awaitable operator co_await() noexcept {
		return *this;
	}

private:
	bool IsReady() const noexcept {
		return !cancel_ptr;
	}

	void TakeValue() const noexcept {}

	void OnReady() noexcept {
		cancel_ptr = {};

		if (continuation)
			continuation.resume();
	}
};
