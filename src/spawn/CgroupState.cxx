// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupState.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "io/MakeDirectory.hxx"
#include "io/Open.hxx"
#include "io/SmallTextFile.hxx"
#include "io/WriteFile.hxx"
#include "io/linux/ProcCgroup.hxx"
#include "util/IterableSplitString.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

#include <fmt/format.h>

#include <concepts>
#include <span>

#include <fcntl.h>
#include <sys/stat.h>

using std::string_view_literals::operator""sv;

CgroupState::CgroupState() noexcept {}
CgroupState::~CgroupState() noexcept = default;

static void
WriteFile(FileDescriptor fd, const char *path, std::string_view data)
{
	if (TryWriteExistingFile(fd, path, data) == WriteFileResult::ERROR)
		throw FmtErrno("write('{}') failed", path);
}

static void
ForEachController(FileDescriptor group_fd,
		  std::invocable<std::string_view> auto callback)
{
	WithSmallTextFile<1024>(FileAt{group_fd, "cgroup.controllers"}, [&callback](std::string_view contents){
		if (contents.back() == '\n')
			contents.remove_suffix(1);

		for (const auto name : IterableSplitString(contents, ' '))
			if (!name.empty())
				callback(name);
	});
}

void
CgroupState::EnableAllControllers(unsigned pid) const
{
	assert(IsEnabled());

	/* create a leaf cgroup and move this process into it, or else
	   we can't enable other controllers */

	const auto leaf_group = MakeDirectory(group_fd, "_", {.mode=0700});
	WriteFile(leaf_group, "cgroup.procs", fmt::format_int{pid}.c_str());

	/* now enable all other controllers in subtree_control */

	std::string subtree_control;
	ForEachController(group_fd, [&subtree_control](const auto controller){
		if (controller == "cpuset")
			/* ignoring the "cpuset" controller because we
			   never used it and its cpuset_css_online()
			   function adds 70ms delay */
			// TODO make this a runtime configuration
			return;

		if (!subtree_control.empty())
			subtree_control.push_back(' ');
		subtree_control.push_back('+');
		subtree_control += controller;
	});

	WriteFile(group_fd, "cgroup.subtree_control", subtree_control);
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
CgroupState::FromGroupPath(std::string &&group_path)
{
	assert(!group_path.empty());

	CgroupState state;

	auto sys_fs_cgroup = OpenPath("/sys/fs/cgroup");

	state.group_fd = OpenPath(sys_fs_cgroup, group_path.c_str() + 1);
	state.cgroup_kill = HasCgroupKill(state.group_fd);

	state.group_path = std::move(group_path);

	return state;
}

CgroupState
CgroupState::FromProcess(unsigned pid)
{
	auto group_path = ReadProcessCgroup(pid);
	if (group_path.empty())
		return {};

	return FromGroupPath(std::move(group_path));
}

CgroupState
CgroupState::FromProcess(unsigned pid, std::string override_group_path)
{
	if (auto group_path = ReadProcessCgroup(pid); group_path.empty())
		return {};

	return FromGroupPath(std::move(override_group_path));
}
