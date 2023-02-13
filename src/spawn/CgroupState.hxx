// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

#include <forward_list>
#include <string>
#include <map>

struct ProcCgroup;

struct CgroupState {
	/**
	 * Our own control group path.  It starts with a slash.
	 */
	std::string group_path;

	/**
	 * A cgroup controller mount point below /sys/fs/cgroup.  A
	 * mount point may contain several controllers.
	 */
	struct Mount {
		/**
		 * An empty string means this is a cgroup2 "unified"
		 * mount on /sys/fs/cgroup.
		 */
		std::string name;

		/**
		 * An O_PATH file descriptor of the group managed by
		 * us (delegated from systemd).
		 */
		UniqueFileDescriptor fd;

#ifdef __clang__
		template<typename N>
		Mount(N &&_name, UniqueFileDescriptor &&_fd) noexcept
			:name(std::forward<N>(_name)), fd(std::move(_fd)) {}
#endif
	};

	/**
	 * The controller mount points below /sys/fs/cgroup which are
	 * managed by us (delegated from systemd).
	 *
	 * A single item with an empty name means there is a cgroup2 "unified"
	 * mount on /sys/fs/cgroup.
	 */
	std::forward_list<Mount> mounts;

	/**
	 * A mapping from controller name to mount point name.  More than
	 * one controller may be mounted at one mount point.
	 */
	std::map<std::string, std::string, std::less<>> controllers;

	/**
	 * Is the "memory" cgroup controller using the cgroup2 interface?
	 */
	bool memory_v2 = false;

	/**
	 * Does the kernel support "cgroup.kill"?
	 */
	bool cgroup_kill = false;

	CgroupState() noexcept;
	~CgroupState() noexcept;

	CgroupState(CgroupState &&) noexcept = default;
	CgroupState &operator=(CgroupState &&) noexcept = default;

	bool IsEnabled() const noexcept {
		return !group_path.empty();
	}

	[[gnu::pure]]
	bool IsV2() const noexcept;

	/**
	 * Returns an O_PATH file descriptor to our group in the
	 * cgroup2 mount or FileDescriptor::Undefined() if none was
	 * mounted.
	 */
	FileDescriptor GetUnifiedGroupMount() const noexcept;

	/**
	 * Enable all controllers for newly created groups by writing
	 * to "subtree_control".
	 *
	 * Throws on error.
	 */
	void EnableAllControllers() const;

	/**
	 * Obtain cgroup membership information from the cgroups
	 * assigned by systemd to the specified process, and return it
	 * as a #CgroupState instance.
	 *
	 * Throws on error.
	 *
	 * @param pid the process id or 0 for the current process
	 */
	static CgroupState FromProcess(unsigned pid=0);

	/**
	 * This overload takes a custom group path.
	 */
	static CgroupState FromProcess(unsigned pid, std::string group_path);

private:
	/**
	 * Throws on error.
	 */
	[[gnu::pure]]
	static CgroupState FromProcCgroup(ProcCgroup &&proc_cgroup);
};
