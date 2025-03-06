// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/SharedLease.hxx"

class FileDescriptor;

/**
 * Pointer to a watched cgroup managed by #CgroupMultiWatch.
 */
class CgroupWatchPtr {
	friend class CgroupMultiWatch;

	SharedLease lease;

	constexpr CgroupWatchPtr(SharedAnchor &anchor) noexcept
		:lease(anchor) {}

public:
	constexpr CgroupWatchPtr() noexcept = default;
	constexpr CgroupWatchPtr(const CgroupWatchPtr &) noexcept = default;
	constexpr CgroupWatchPtr(CgroupWatchPtr &&) noexcept = default;
	CgroupWatchPtr &operator=(const CgroupWatchPtr &) noexcept = default;
	CgroupWatchPtr &operator=(CgroupWatchPtr &&) noexcept = default;

	constexpr operator bool() const noexcept {
		return static_cast<bool>(lease);
	}

	/**
	 * Is this cgroup currently blocked? (because it recently went
	 * over limits)
	 */
	[[gnu::pure]]
	bool IsBlocked() const noexcept;

	void SetCgroup(FileDescriptor cgroup_fd) noexcept;
};
