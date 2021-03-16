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

		ControllerAssignment(StringView _name, StringView _path)
			:name(_name.data, _name.size),
			 path(_path.data, _path.size) {}
	};

	std::forward_list<ControllerAssignment> assignments;

	std::string systemd_path;
	bool have_unified = false;

	char line[256];
	while (fgets(line, sizeof(line), file) != nullptr) {
		if (StringStartsWith(line, "0::/")) {
			have_unified = true;
			continue;
		}

		char *p = line, *endptr;

		strtoul(p, &endptr, 10);
		if (endptr == p || *endptr != ':')
			continue;

		char *const _name = endptr + 1;
		char *const colon = strchr(_name, ':');
		if (colon == nullptr || colon == _name ||
		    colon[1] != '/' || colon[2] == '/')
			continue;

		StringView name(_name, colon);

		StringView path(colon + 1);
		if (path.back() == '\n')
			--path.size;

		if (name.Equals("name=systemd"))
			systemd_path = std::string(path.data, path.size);
		else {
			assignments.emplace_front(name, path);

			auto &controllers = assignments.front().controllers;
			for (StringView i : IterableSplitString(name, ','))
				controllers.emplace_front(i.data, i.size);
		}
	}

	if (systemd_path.empty())
		/* no "systemd" controller found - disable the feature */
		return CgroupState();

	CgroupState state;

	for (auto &i : assignments) {
		if (i.path == systemd_path) {
			for (auto &controller : i.controllers)
				state.controllers.emplace(std::move(controller), i.name);

			state.mounts.emplace_front(std::move(i.name));
		}
	}

	state.mounts.emplace_front("systemd");

	if (have_unified)
		state.mounts.emplace_front("unified");

	// TODO: support pure unified hierarchy (no hybrid)

	state.group_path = std::move(systemd_path);

	return state;
}
