// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

#include <string>

struct ProcCgroup;

struct CgroupState {
	/**
	 * Our own control group path.  It starts with a slash.
	 */
	std::string group_path;

	/**
	 * An O_PATH file descriptor of the group managed by us
	 * (delegated from systemd).
	 */
	UniqueFileDescriptor group_fd;

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
