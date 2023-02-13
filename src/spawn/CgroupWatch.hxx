// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/Chrono.hxx"
#include "event/InotifyEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <cstdint>

struct CgroupState;

class CgroupMemoryWatch final : InotifyHandler {
	UniqueFileDescriptor fd;

	InotifyEvent inotify;

	Event::TimePoint next_time;

	BoundMethod<void(uint64_t value) noexcept> callback;

public:
	/**
	 * Throws if the group memory usage file could not be opened.
	 */
	CgroupMemoryWatch(EventLoop &event_loop,
			  const CgroupState &state,
			  BoundMethod<void(uint64_t value) noexcept> _callback);

	auto &GetEventLoop() const noexcept {
		return inotify.GetEventLoop();
	}

private:
	/* virtual methods from class InotifyHandler */
	void OnInotify(int wd, unsigned mask, const char *name) override;
	void OnInotifyError(std::exception_ptr error) noexcept override;
};
