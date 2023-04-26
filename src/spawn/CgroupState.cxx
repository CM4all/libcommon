// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupState.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "system/Error.hxx"
#include "io/MakeDirectory.hxx"
#include "io/Open.hxx"
#include "io/WithFile.hxx"
#include "io/WriteFile.hxx"
#include "util/IterableSplitString.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

#include <span>

#include <fcntl.h>
#include <sys/stat.h>

using std::string_view_literals::operator""sv;

CgroupState::CgroupState() noexcept {}
CgroupState::~CgroupState() noexcept = default;

static std::size_t
ReadFile(FileAt file, std::span<std::byte> dest)
{
	return WithReadOnly(file, [dest](auto fd){
		auto nbytes = fd.Read(dest.data(), dest.size());
		if (nbytes < 0)
			throw MakeErrno("Failed to read");
		return static_cast<std::size_t>(nbytes);
	});
}

static std::string_view
ReadTextFile(FileAt file, std::span<char> dest)
{
	const auto size = ReadFile(file, std::as_writable_bytes(dest));
	return {dest.data(), size};
}

static void
WriteFile(FileDescriptor fd, const char *path, std::string_view data)
{
	if (TryWriteExistingFile(fd, path, data) == WriteFileResult::ERROR)
		throw FormatErrno("write('%s') failed", path);
}

static void
ForEachController(FileDescriptor group_fd, auto &&callback)
{
	char buffer[1024];

	auto contents = ReadTextFile({group_fd, "cgroup.controllers"}, buffer);
	if (contents.empty())
		return;

	if (contents.back() == '\n')
		contents.remove_suffix(1);

	for (const auto name : IterableSplitString(contents, ' '))
		if (!name.empty())
			callback(name);
}

void
CgroupState::EnableAllControllers() const
{
	assert(IsEnabled());

	/* create a leaf cgroup and move this process into it, or else
	   we can't enable other controllers */

	const auto leaf_group = MakeDirectory(group_fd, "_", 0700);
	WriteFile(leaf_group, "cgroup.procs", "0");

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

	/* attempt to give the spawner the highest possible CPU and
	   I/O weight; the spawner is more important than its child
	   processes */
	TryWriteExistingFile(leaf_group, "cpu.weight", "10000");
	TryWriteExistingFile(leaf_group, "io.weight", "10000");
	TryWriteExistingFile(leaf_group, "io.bfq.weight", "1000");
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
	std::string group_path;
	bool have_unified = false, have_systemd = false, have_v1 = false;

	bool IsDefined() const noexcept {
		return !group_path.empty();
	}
};

static FILE *
OpenProcCgroup(unsigned pid)
{
	if (pid > 0)
		return OpenOrThrow(FmtBuffer<64>("/proc/{}/cgroup", pid),
				   "r");
	else
		return OpenOrThrow("/proc/self/cgroup", "r");

}

static ProcCgroup
ReadProcCgroup(FILE *file)
{
	ProcCgroup pc;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), file) != nullptr) {
		std::string_view line{buffer};
		line = StripRight(line);

		/* skip the hierarchy id */
		line = Split(line, ':').second;

		const auto [name, path] = Split(line, ':');
		if (path.size() < 2 || path.front() != '/')
			/* ignore malformed lines and lines in the
			   root cgroup */
			continue;

		if (name == "name=systemd"sv) {
			pc.have_systemd = true;
		} else if (name.empty()) {
			pc.group_path = path;
			pc.have_unified = true;
		} else {
			pc.have_v1 = true;
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

	state.group_fd = OpenPath(sys_fs_cgroup, proc_cgroup.group_path.c_str() + 1);
	state.cgroup_kill = HasCgroupKill(state.group_fd);

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
