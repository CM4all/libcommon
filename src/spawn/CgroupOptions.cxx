// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupOptions.hxx"
#include "CgroupState.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "io/MakeDirectory.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/WriteFile.hxx"
#include "util/StringAPI.hxx"

#include <fmt/format.h>

#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/xattr.h>

CgroupOptions::CgroupOptions(AllocatorPtr alloc,
			     const CgroupOptions &src) noexcept
	:name(alloc.CheckDup(src.name)),
	 xattr(alloc, src.xattr),
	 set(alloc, src.set)
{
}

void
CgroupOptions::SetXattr(AllocatorPtr alloc,
			std::string_view _name, std::string_view _value) noexcept
{
	xattr.Add(alloc, _name, _value);
}

void
CgroupOptions::Set(AllocatorPtr alloc,
		   std::string_view _name, std::string_view _value) noexcept
{
	set.Add(alloc, _name, _value);
}

static void
WriteFile(FileDescriptor fd, const char *path, std::string_view data)
{
	if (TryWriteExistingFile(fd, path, data) == WriteFileResult::ERROR)
		throw FormatErrno("write('%s') failed", path);
}

static void
WriteCgroupFile(FileDescriptor group_fd,
		const char *filename, std::string_view value)
{
	/* emulate cgroup1 for old translation servers */
	if (StringIsEqual(filename, "memory.limit_in_bytes"))
		filename = "memory.max";

	WriteFile(group_fd, filename, value);
}

UniqueFileDescriptor
CgroupOptions::Create2(const CgroupState &state) const
{
	if (name == nullptr)
		return {};

	if (!state.IsEnabled())
		throw std::runtime_error("Control groups are disabled");

	auto fd = MakeDirectory(state.group_fd, name);

	if (!xattr.empty()) {
		/* reopen the directory because fsetxattr() refuses to
		   work with an O_PATH fildescriptor */
		auto d = OpenDirectory(fd, ".");

		for (const auto &i : xattr)
			if (fsetxattr(d.Get(), i.name,
				      i.value, strlen(i.value), 0) < 0)
				throw FormatErrno("Failed to set xattr '%s'",
						  i.name);
	}

	for (const auto &s : set)
		WriteCgroupFile(fd, s.name, s.value);

	if (session != nullptr)
		return MakeDirectory(fd, session);
	else
		return fd;
}

static UniqueFileDescriptor
MoveToNewCgroup(const FileDescriptor group_fd,
		const char *sub_group, const char *session_group,
		std::string_view pid)
{
	auto fd = MakeDirectory(group_fd, sub_group);

	if (session_group != nullptr) {
		auto session_fd = MakeDirectory(fd, session_group);
		WriteFile(session_fd, "cgroup.procs", pid);
	} else
		WriteFile(fd, "cgroup.procs", pid);

	return fd;
}

void
CgroupOptions::Apply(const CgroupState &state, unsigned _pid) const
{
	if (name == nullptr)
		return;

	if (!state.IsEnabled())
		throw std::runtime_error("Control groups are disabled");

	const fmt::format_int pid_buffer{_pid};
	const std::string_view pid{pid_buffer.data(), pid_buffer.size()};

	/* TODO drop support for cgroup1 and hybrid, use only
	   Create2() and CLONE_INTO_CGROUP */

	auto fd = MoveToNewCgroup(state.group_fd, name, session, pid);

	for (const auto &s : set)
		WriteCgroupFile(fd, s.name, s.value);
}

char *
CgroupOptions::MakeId(char *p) const noexcept
{
	if (name != nullptr) {
		p = (char *)mempcpy(p, ";cg", 3);
		p = stpcpy(p, name);

		if (session != nullptr) {
			*p++ = '/';
			p = stpcpy(p, session);
		}
	}

	return p;
}
