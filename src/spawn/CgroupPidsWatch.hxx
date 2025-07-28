// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/InotifyEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <cstdint>

/**
 * Watch the "pids.events" file and invoke a callback with the
 * "pids.current" value whenever "pids.events" changes.
 */
class CgroupPidsWatch final : InotifyHandler {
	UniqueFileDescriptor current_fd;

	InotifyEvent inotify;

	BoundMethod<void(uint_least64_t pids_current) noexcept> callback;

public:
	/**
	 * Throws if the group pids files could not be opened.
	 *
	 * @param group_fd a file descriptor to the cgroup directory
	 * to be watched
	 */
	CgroupPidsWatch(EventLoop &event_loop,
			FileDescriptor group_fd,
			BoundMethod<void(uint_least64_t pids_current) noexcept> _callback);

	~CgroupPidsWatch() noexcept;

	auto &GetEventLoop() const noexcept {
		return inotify.GetEventLoop();
	}

	/**
	 * Determines the current number of pids.
	 *
	 * Throws on error.
	 */
	uint_least64_t GetPidsCurrent() const;

private:
	/* virtual methods from class InotifyHandler */
	void OnInotify(int wd, unsigned mask, const char *name) override;
	void OnInotifyError(std::exception_ptr error) noexcept override;
};
