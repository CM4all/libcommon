// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupOptions.hxx"
#include "CgroupState.hxx"
#include "AllocatorPtr.hxx"
#include "lib/fmt/SystemError.hxx"
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
		throw FmtErrno("write({:?}) failed", path);
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
CgroupOptions::Create2(const CgroupState &state, const char *session) const
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
				throw FmtErrno("Failed to set xattr {:?}",
					       i.name);
	}

	for (const auto &s : set)
		WriteCgroupFile(fd, s.name, s.value);

	if (session != nullptr)
		return MakeDirectory(fd, session);
	else
		return fd;
}

char *
CgroupOptions::MakeId(char *p) const noexcept
{
	if (name != nullptr) {
		p = (char *)mempcpy(p, ";cg", 3);
		p = stpcpy(p, name);
	}

	return p;
}
