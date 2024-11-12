// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "util/BindMethod.hxx"

#include <atomic>

/**
 * Send notifications from a worker thread to the main thread.
 */
class Notify {
	typedef BoundMethod<void() noexcept> Callback;
	Callback callback;

	PipeEvent event;

	std::atomic_bool pending{false};

public:
	Notify(EventLoop &event_loop, Callback _callback) noexcept;
	~Notify() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Enable() noexcept {
		event.ScheduleRead();
	}

	void Disable() noexcept {
		event.Cancel();
	}

	void Signal() noexcept;

private:
	void EventFdCallback(unsigned events) noexcept;
};
