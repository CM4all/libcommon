// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "PipeEvent.hxx"
#include "util/BindMethod.hxx"

#include <assert.h>
#include <signal.h>

/**
 * Listen for signals delivered to this process, and then invoke a
 * callback.
 *
 * After constructing an instance, call Add() to add signals to listen
 * on.  When done, call Enable().  After that, Add() must not be
 * called again.
 */
class SignalEvent {
	PipeEvent event;

	sigset_t mask;

	using Callback = BoundMethod<void(int) noexcept>;
	const Callback callback;

public:
	SignalEvent(EventLoop &loop, Callback _callback) noexcept;

	SignalEvent(EventLoop &loop, int signo, Callback _callback) noexcept
		:SignalEvent(loop, _callback) {
		Add(signo);
	}

	~SignalEvent() noexcept {
		Disable();
	}

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	bool IsDefined() const noexcept {
		return event.IsDefined();
	}

	void Add(int signo) noexcept {
		assert(!IsDefined());

		sigaddset(&mask, signo);
	}

	void Enable();
	void Disable() noexcept;

private:
	void EventCallback(unsigned events) noexcept;
};
