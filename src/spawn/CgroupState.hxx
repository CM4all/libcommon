/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BENG_PROXY_CGROUP_STATE_HXX
#define BENG_PROXY_CGROUP_STATE_HXX

#include <forward_list>
#include <string>
#include <map>

struct CgroupState {
	/**
	 * Our own control group path.  It starts with a slash.
	 */
	std::string group_path;

	/**
	 * The controller mount points below /sys/fs/cgroup which are
	 * managed by us (delegated from systemd).  Each mount point may
	 * contain several controllers.
	 */
	std::forward_list<std::string> mounts;

	/**
	 * A mapping from controller name to mount point name.  More than
	 * one controller may be mounted at one mount point.
	 */
	std::map<std::string, std::string> controllers;

	bool IsEnabled() const {
		return !group_path.empty();
	}

	/**
	 * Returns the absolute path where the cgroup2 is mounted or
	 * an empty string if none was mounted.
	 */
	[[gnu::pure]]
	std::string GetUnifiedMount() const noexcept;

	/**
	 * Obtain cgroup membership information from the cgroups
	 * assigned by systemd to the specified process, and return it
	 * as a #CgroupState instance.  Returns an empty #CgroupState
	 * on error.
	 *
	 * @param pid the process id or 0 for the current process
	 */
	[[gnu::pure]]
	static CgroupState FromProcess(unsigned pid=0) noexcept;
};

#endif
