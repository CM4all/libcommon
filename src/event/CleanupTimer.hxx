// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "FarTimerEvent.hxx"

/**
 * Wrapper for #CoarseTimerEvent which aims to simplify installing recurring
 * events.
 */
class CleanupTimer {
	FarTimerEvent event;

	const Event::Duration delay;

	/**
	 * @return true if another cleanup shall be scheduled
	 */
	using Callback = BoundMethod<bool() noexcept>;
	const Callback callback;

public:
	CleanupTimer(EventLoop &loop, Event::Duration _delay,
		     Callback _callback) noexcept
		:event(loop, BIND_THIS_METHOD(OnTimer)),
		 delay(_delay),
		 callback(_callback) {}

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Enable() noexcept;
	void Disable() noexcept;

private:
	void OnTimer() noexcept;
};
