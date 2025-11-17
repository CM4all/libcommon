// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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

	/**
	 * Wait for Signal() to be called synchronously, but do not
	 * consume the event.  This method is only meant as a
	 * workaround for unit tests and should not be used in regular
	 * code.
	 */
	void WaitSynchronously() noexcept {
		(void)event.GetFileDescriptor().WaitReadable(-1);
	}

private:
	void EventFdCallback(unsigned events) noexcept;
};
