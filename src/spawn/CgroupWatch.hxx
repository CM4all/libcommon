// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/InotifyEvent.hxx"
#include "event/PipeEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <cstdint>

class CgroupMemoryWatch final : InotifyHandler {
	UniqueFileDescriptor fd;

	InotifyEvent inotify;

	/**
	 * Subscription for "memory.pressure".
	 */
	PipeEvent pressure;

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

	~CgroupMemoryWatch() noexcept;

	auto &GetEventLoop() const noexcept {
		return inotify.GetEventLoop();
	}

	/**
	 * Determines the current memory usage.
	 *
	 * Throws on error.
	 */
	uint_least64_t GetMemoryUsage() const;

private:
	void OnPressure(unsigned events) noexcept;

	/* virtual methods from class InotifyHandler */
	void OnInotify(int wd, unsigned mask, const char *name) override;
	void OnInotifyError(std::exception_ptr error) noexcept override;
};
