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

#include "CgroupState.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/IterableSplitString.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringCompare.hxx"

#include <cstdio>

static FILE *
OpenProcCgroup(unsigned pid) noexcept
{
	if (pid > 0) {
		char buffer[256];
		sprintf(buffer, "/proc/%u/cgroup", pid);
		return fopen(buffer, "r");
	} else
		return fopen("/proc/self/cgroup", "r");

}

CgroupState
CgroupState::FromProcess(unsigned pid) noexcept
{
	FILE *file = OpenProcCgroup(pid);
	if (file == nullptr)
		return CgroupState();

	AtScopeExit(file) { fclose(file); };

	struct ControllerAssignment {
		std::string name;
		std::string path;

		std::forward_list<std::string> controllers;

		ControllerAssignment(std::string_view _name,
				     std::string_view _path) noexcept
			:name(_name), path(_path) {}
	};

	std::forward_list<ControllerAssignment> assignments;

	std::string group_path;
	bool have_unified = false, have_systemd = false;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), file) != nullptr) {
		StringView line(buffer);
		line.StripRight();

		/* skip the hierarchy id */
		line = line.Split(':').second;

		const auto [name, path] = line.Split(':');
		if (path.size < 2 || path.front() != '/')
			/* ignore malformed lines and lines in the
			   root cgroup */
			continue;

		if (name.Equals("name=systemd")) {
			group_path = path;
			have_systemd = true;
		} else if (name.empty()) {
			group_path = path;
			have_unified = true;
		} else {
			assignments.emplace_front(name, path);

			auto &controllers = assignments.front().controllers;
			for (std::string_view i : IterableSplitString(name, ','))
				controllers.emplace_front(i);
		}
	}

	if (group_path.empty())
		/* no "systemd" controller found - disable the feature */
		return CgroupState();

	CgroupState state;

	for (auto &i : assignments) {
		if (i.path == group_path) {
			for (auto &controller : i.controllers)
				state.controllers.emplace(std::move(controller), i.name);

			state.mounts.emplace_front(std::move(i.name));
		}
	}

	if (have_systemd)
		state.mounts.emplace_front("systemd");

	if (have_unified) {
		std::string_view unified_mount = "unified";
		if (state.mounts.empty()) {
			UniqueFileDescriptor fd;
			if (!fd.OpenReadOnly("/sys/fs/cgroup/unified/cgroup.controllers")) {
				unified_mount = {};
				fd.OpenReadOnly("/sys/fs/cgroup/cgroup.controllers");
			}

			if (fd.IsDefined()) {
				ssize_t nbytes = fd.Read(buffer, sizeof(buffer));
				if (nbytes > 0) {
					if (buffer[nbytes - 1] == '\n')
						--nbytes;

					const StringView contents(buffer, nbytes);
					for (std::string_view name : IterableSplitString(contents, ' '))
						if (!name.empty())
							state.controllers.emplace(name, unified_mount);
				}
			}
		}

		state.mounts.emplace_front(unified_mount);
	}

	state.group_path = std::move(group_path);

	return state;
}
