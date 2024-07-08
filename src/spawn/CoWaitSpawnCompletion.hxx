// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "ProcessHandle.hxx"
#include "CompletionHandler.hxx"
#include "co/AwaitableHelper.hxx"

/**
 * Coroutine adapter for ChildProcessHandle::SetCompletionHandler().
 * The awaiting coroutine is resumed as soon as spawning completes.
 */
class CoWaitSpawnCompletion final : SpawnCompletionHandler {
	std::coroutine_handle<> continuation;

	using Awaitable = Co::AwaitableHelper<CoWaitSpawnCompletion, true>;
	friend Awaitable;

	std::exception_ptr error;

	bool ready = false;

public:
	[[nodiscard]]
	explicit CoWaitSpawnCompletion(ChildProcessHandle &handle) noexcept
	{
		handle.SetCompletionHandler(*this);
	}

	CoWaitSpawnCompletion(const CoWaitSpawnCompletion &) = delete;
	CoWaitSpawnCompletion &operator=(const CoWaitSpawnCompletion &) = delete;

	[[nodiscard]]
	Awaitable operator co_await() noexcept {
		return *this;
	}

private:
	bool IsReady() const noexcept {
		return ready;
	}

	void TakeValue() const noexcept {}

	/* virtual methods from class SpawnCompletionHandler */
	void OnSpawnSuccess() noexcept override {
		assert(!ready);
		assert(!error);

		ready = true;

		if (continuation)
			continuation.resume();
	}

	void OnSpawnError(std::exception_ptr _error) noexcept override {
		assert(!ready);
		assert(!error);

		ready = true;
		error = std::move(_error);

		if (continuation)
			continuation.resume();
	}
};
