// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/Chrono.hxx"
#include "event/InotifyEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <cstdint>

class CgroupMemoryWatch final : InotifyHandler {
	UniqueFileDescriptor fd;

	InotifyEvent inotify;

	Event::TimePoint next_time;

	BoundMethod<void(uint_least64_t value) noexcept> callback;

public:
	/**
	 * Throws if the group memory usage file could not be opened.
	 *
	 * @param group_fd a file descriptor to the cgroup directory
	 * to be watched
	 */
	CgroupMemoryWatch(EventLoop &event_loop,
			  FileDescriptor group_fd,
			  BoundMethod<void(uint_least64_t value) noexcept> _callback);

	auto &GetEventLoop() const noexcept {
		return inotify.GetEventLoop();
	}

private:
	/* virtual methods from class InotifyHandler */
	void OnInotify(int wd, unsigned mask, const char *name) override;
	void OnInotifyError(std::exception_ptr error) noexcept override;
};
