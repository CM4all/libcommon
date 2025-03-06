// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/InotifyManager.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/StringWithHash.hxx"

class CgroupWatchPtr;

/**
 * Watches events on a bunch of cgroups identified by their relative
 * path names.
 *
 * To use it, call Get() to obtain a #CgroupWatchPtr.  Once you have
 * created the cgroup, pass a file descriptor of the cgroup directory
 * to CgroupWatchPtr::SetCgroup().
 */
class CgroupMultiWatch final {
	friend class CgroupWatchPtr;

	InotifyManager inotify_manager;

	struct Item;

	struct GetName {
		constexpr StringWithHash operator()(const Item &item) const noexcept;
	};

	IntrusiveHashSet<Item, 4096,
			 IntrusiveHashSetOperators<Item, GetName,
						   std::hash<StringWithHash>,
						   std::equal_to<StringWithHash>>> items;

public:
	explicit CgroupMultiWatch(EventLoop &event_loop);
	~CgroupMultiWatch() noexcept;

	auto &GetEventLoop() const noexcept {
		return inotify_manager.GetEventLoop();
	}

	/**
	 * Initiate shutdown.  This unregisters all #EventLoop events
	 * and prevents new ones from getting registered.
	 */
	void BeginShutdown() noexcept;

	/**
	 * Has BeginShutdown() been called?
	 */
	bool IsShuttingDown() const noexcept {
		return inotify_manager.IsShuttingDown();
	}

	/**
	 * Start watching the specified cgroup and return a
	 * #CgroupWatchPtr referring to it.  To activate the returned
	 * value, its SetCgroup() method must be called.
	 */
	CgroupWatchPtr Get(StringWithHash name) noexcept;
};
