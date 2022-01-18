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
#include "system/Error.hxx"
#include "io/Open.hxx"
#include "io/WriteFile.hxx"
#include "util/IterableSplitString.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringCompare.hxx"

#include <cstdio>

#include <fcntl.h>
#include <sys/stat.h>

CgroupState::CgroupState() noexcept {}
CgroupState::~CgroupState() noexcept = default;

bool
CgroupState::IsV2() const noexcept
{
	return !mounts.empty() && mounts.front().name.empty();
}

static void
WriteFile(FileDescriptor fd, const char *path, std::string_view data)
{
	if (TryWriteExistingFile(fd, path, data) == WriteFileResult::ERROR)
		throw FormatErrno("write('%s') failed", path);
}

std::string
CgroupState::GetUnifiedMountPath() const noexcept
{
	if (mounts.empty())
		return {};

	const auto &i = mounts.front().name;
	if (i.empty())
		return "/sys/fs/cgroup";

	if (i == "unified")
		return "/sys/fs/cgroup/unified";

	return {};
}

FileDescriptor
CgroupState::GetUnifiedMount() const noexcept
{
	if (mounts.empty())
		return FileDescriptor::Undefined();

	if (const auto &m = mounts.front();
	    m.name.empty() || m.name == "unified")
		return m.fd;

	return FileDescriptor::Undefined();
}

void
CgroupState::EnableAllControllers() const
{
	if (!IsV2())
		/* this is only relevant if only V2 is mounted (not
		   cgroup1 and not hybrid) */
		return;

	const FileDescriptor spawn_group = mounts.front().fd;

	/* create a leaf cgroup and move this process into it, or else
	   we can't enable other controllers */

	const char *leaf_name = "_";
	if (mkdirat(spawn_group.Get(), leaf_name, 0700) < 0)
		throw MakeErrno("Failed to create spawner leaf cgroup");

	{
		auto leaf_group = OpenPath(spawn_group, leaf_name);
		WriteFile(leaf_group, "cgroup.procs", "0");
	}

	/* now enable all other controllers in subtree_control */

	std::string subtree_control;
	for (const auto &[controller, mount] : controllers) {
		if (!mount.empty())
			continue;

		if (controller == "cpuset")
			/* ignoring the "cpuset" controller because we
			   never used it and its cpuset_css_online()
			   function adds 70ms delay */
			// TODO make this a runtime configuration
			continue;

		if (!subtree_control.empty())
			subtree_control.push_back(' ');
		subtree_control.push_back('+');
		subtree_control += controller;
	}

	WriteFile(spawn_group, "cgroup.subtree_control", subtree_control);
}

static FILE *
OpenOrThrow(const char *path, const char *mode)
{
	FILE *file = fopen(path, mode);
	if (file == nullptr)
		throw FormatErrno("Failed to open %s", path);

	return file;
}

struct ProcCgroup {
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

	bool IsDefined() const noexcept {
		return !group_path.empty();
	}
};

static FILE *
OpenProcCgroup(unsigned pid)
{
	if (pid > 0) {
		char buffer[256];
		sprintf(buffer, "/proc/%u/cgroup", pid);
		return OpenOrThrow(buffer, "r");
	} else
		return OpenOrThrow("/proc/self/cgroup", "r");

}

static ProcCgroup
ReadProcCgroup(FILE *file)
{
	ProcCgroup pc;

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
			pc.group_path = path;
			pc.have_systemd = true;
		} else if (name.empty()) {
			pc.group_path = path;
			pc.have_unified = true;
		} else {
			pc.assignments.emplace_front(name, path);

			auto &controllers = pc.assignments.front().controllers;
			for (std::string_view i : IterableSplitString(name, ','))
				controllers.emplace_front(i);
		}
	}

	return pc;
}

static ProcCgroup
ReadProcCgroup(unsigned pid)
{
	FILE *file = OpenProcCgroup(pid);
	AtScopeExit(file) { fclose(file); };

	return ReadProcCgroup(file);
}

[[gnu::pure]]
static bool
HasCgroupKill(FileDescriptor fd) noexcept
{
	struct stat st;
	return fstatat(fd.Get(), "cgroup.kill", &st, 0) == 0 &&
		S_ISREG(st.st_mode);
}

CgroupState
CgroupState::FromProcCgroup(ProcCgroup &&proc_cgroup)
{
	if (!proc_cgroup.IsDefined())
		/* no "systemd" controller found - disable the feature */
		return {};

	CgroupState state;

	auto sys_fs_cgroup = OpenPath("/sys/fs/cgroup");

	for (auto &i : proc_cgroup.assignments) {
		if (i.path == proc_cgroup.group_path) {
			for (auto &controller : i.controllers)
				state.controllers.emplace(std::move(controller), i.name);

			auto controller_fd = OpenPath(sys_fs_cgroup,
						      i.name.c_str());
			state.mounts.emplace_front(std::move(i.name),
						   OpenPath(controller_fd,
							    proc_cgroup.group_path.c_str() + 1));
		}
	}

	if (proc_cgroup.have_systemd)
		state.mounts.emplace_front("systemd",
					   OpenPath(sys_fs_cgroup, "systemd"));

	if (proc_cgroup.have_unified) {
		std::string_view unified_mount = "unified";
		if (state.mounts.empty()) {
			UniqueFileDescriptor fd;
			if (!fd.OpenReadOnly(sys_fs_cgroup, "unified/cgroup.subtree_control")) {
				unified_mount = {};
				fd.OpenReadOnly(sys_fs_cgroup, "cgroup.subtree_control");
			}

			if (fd.IsDefined()) {
				char buffer[1024];
				ssize_t nbytes = fd.Read(buffer, sizeof(buffer));
				if (nbytes > 0) {
					if (buffer[nbytes - 1] == '\n')
						--nbytes;

					const StringView contents(buffer, nbytes);
					for (std::string_view name : IterableSplitString(contents, ' ')) {
						if (!name.empty())
							state.controllers.emplace(name, unified_mount);

						if (name == "memory")
							state.memory_v2 = true;
					}
				}
			}
		}

		auto &mount = state.mounts.emplace_front(std::string{unified_mount},
							 OpenPath(sys_fs_cgroup,
								  proc_cgroup.group_path.c_str() + 1));

		state.cgroup_kill = HasCgroupKill(mount.fd);
	}

	state.group_path = std::move(proc_cgroup.group_path);

	return state;
}

CgroupState
CgroupState::FromProcess(unsigned pid)
{
	return FromProcCgroup(ReadProcCgroup(pid));
}

CgroupState
CgroupState::FromProcess(unsigned pid, std::string group_path)
{
	auto proc_cgroup = ReadProcCgroup(pid);
	if (proc_cgroup.IsDefined())
		proc_cgroup.group_path = std::move(group_path);
	return FromProcCgroup(std::move(proc_cgroup));
}
