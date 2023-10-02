// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Mount.hxx"
#include "TmpfsCreate.hxx"
#include "VfsBuilder.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "system/Mount.hxx"
#include "system/openat2.h"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/SpanCast.hxx"
#include "util/StringSplit.hxx"
#include "AllocatorPtr.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <fmt/format.h>

#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>

inline
Mount::Mount(AllocatorPtr alloc, const Mount &src) noexcept
	:source(alloc.CheckDup(src.source)),
	 target(alloc.Dup(src.target)),
	 type(src.type),
#if TRANSLATION_ENABLE_EXPAND
	 expand_source(src.expand_source),
#endif
	 writable(src.writable),
	 exec(src.exec),
	 optional(src.optional) {}

IntrusiveForwardList<Mount>
Mount::CloneAll(AllocatorPtr alloc, const IntrusiveForwardList<Mount> &src) noexcept
{
	IntrusiveForwardList<Mount> dest;
	auto pos = dest.before_begin();

	for (const auto &i : src)
		pos = dest.insert_after(pos, *alloc.New<Mount>(alloc, i));

	return dest;
}

#if TRANSLATION_ENABLE_EXPAND

void
Mount::Expand(AllocatorPtr alloc, const MatchData &match_data)
{
	if (expand_source) {
		expand_source = false;

		source = expand_string_unescaped(alloc, source, match_data);
	}
}

void
Mount::ExpandAll(AllocatorPtr alloc,
		 IntrusiveForwardList<Mount> &list,
		 const MatchData &match_data)
{
	for (auto &i : list)
		i.Expand(alloc, match_data);
}

#endif

/**
 * Open the specified directory as an O_PATH descriptor, but don't
 * follow any symlinks while resolving the given path.
 */
static UniqueFileDescriptor
OpenDirectoryPathNoFollow(FileDescriptor directory, const char *path)
{
	static constexpr struct open_how how{
		.flags = O_PATH|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,
		.resolve = RESOLVE_IN_ROOT|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_SYMLINKS,
	};

	int fd = openat2(directory.Get(), path, &how, sizeof(how));
	if (fd < 0)
		throw FmtErrno("Failed to open '{}'", path);

	return UniqueFileDescriptor{fd};
}

static UniqueFileDescriptor
OpenTreeNoFollow(FileDescriptor directory, const char *path)
{
	return OpenTree(OpenDirectoryPathNoFollow(directory, path), "",
			AT_EMPTY_PATH|OPEN_TREE_CLONE);
}

inline void
Mount::ApplyBindMount(VfsBuilder &vfs_builder) const
{
	if (struct stat st;
	    optional && !source_fd.IsDefined() &&
	    lstat(source, &st) < 0 && errno == ENOENT)
		/* the source directory doesn't exist, but this is
		   optional, so just ignore it */
		return;

	vfs_builder.Add(target);

	uint_least64_t attr_set = MS_NOSUID|MS_NODEV, attr_clr = 0;
	if (writable)
		attr_clr |= MS_RDONLY;
	else
		attr_set |= MS_RDONLY;
	if (exec)
		attr_clr |= MS_NOEXEC;
	else
		attr_set |= MS_NOEXEC;

	if (source_fd.IsDefined())
		MoveMount(source_fd, "",
			  FileDescriptor::Undefined(), target,
			  MOVE_MOUNT_F_EMPTY_PATH);
	else
		MoveMount(OpenTreeNoFollow(FileDescriptor{AT_FDCWD}, source), "",
			  FileDescriptor::Undefined(), target,
			  MOVE_MOUNT_F_EMPTY_PATH);

	MountSetAttr(FileDescriptor::Undefined(), target,
		     AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT,
		     attr_set, attr_clr);
}

inline void
Mount::ApplyBindMountFile(VfsBuilder &vfs_builder) const
{
	if (struct stat st;
	    optional && !source_fd.IsDefined() &&
	    lstat(source, &st) < 0 && errno == ENOENT)
		/* the source file doesn't exist, but this is
		   optional, so just ignore it */
		return;

	if (struct stat st; lstat(target, &st) == 0) {
		/* target exists already */
		if (!S_ISREG(st.st_mode))
			throw FmtRuntimeError("Not a regular file: {}",
					      target);
	} else if (const int e = errno; e != ENOENT) {
		throw FmtErrno(e, "Failed to stat {}", target);
	} else {
		/* target does not exist: first ensure that its parent
		   directory exists, then create an empty target */
		const auto parent = SplitLast(std::string_view{target}, '/').first;
		vfs_builder.MakeDirectory(parent);

		UniqueFileDescriptor fd;
		if (!fd.Open(target, O_CREAT|O_WRONLY, 0666))
			throw FmtErrno("Failed to create {}", target);
	}

	constexpr uint_least64_t attr_set = MS_NOSUID|MS_NODEV|MS_RDONLY|MS_NOEXEC;

	if (source_fd.IsDefined())
		MoveMount(source_fd, "",
			  FileDescriptor::Undefined(), target,
			  MOVE_MOUNT_F_EMPTY_PATH);
	else
		BindMount(source, target);

	MountSetAttr(FileDescriptor::Undefined(), target,
		     AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT, attr_set, 0);
}

inline void
Mount::ApplyTmpfs(VfsBuilder &vfs_builder) const
{
	vfs_builder.Add(target);

	int flags = MS_NOSUID|MS_NODEV;
	if (!exec)
		flags |= MS_NOEXEC;

	auto fs = FSOpen("tmpfs");
	FSConfig(fs, FSCONFIG_SET_STRING, "size", "16M");
	FSConfig(fs, FSCONFIG_SET_STRING, "nr_inodes", "256");
	FSConfig(fs, FSCONFIG_SET_STRING, "mode", "711");

	if (writable) {
		FSConfig(fs, FSCONFIG_SET_STRING, "uid",
			 fmt::format_int(vfs_builder.uid).c_str());
		FSConfig(fs, FSCONFIG_SET_STRING, "gid",
			 fmt::format_int(vfs_builder.gid).c_str());
	}

	FSConfig(fs, FSCONFIG_CMD_CREATE, nullptr, nullptr);

	MoveMount(FSMount(fs, flags), "",
		  FileDescriptor::Undefined(), target,
		  MOVE_MOUNT_F_EMPTY_PATH);

	vfs_builder.MakeWritable();

	if (!writable)
		vfs_builder.ScheduleRemount(MS_RDONLY, 0);
}

inline void
Mount::ApplyNamedTmpfs(VfsBuilder &vfs_builder) const
{
	vfs_builder.Add(target);

	if (source_fd.IsDefined()) {
		MoveMount(source_fd, "",
			  FileDescriptor::Undefined(), target,
			  MOVE_MOUNT_F_EMPTY_PATH);
	} else {
		/* we didn't get a "source_fd", so just create a new
		   one (which will not be shared with anybody, just a
		   fallback) */

		MoveMount(CreateTmpfs(exec), "",
			  FileDescriptor::Undefined(), target,
			  MOVE_MOUNT_F_EMPTY_PATH);
	}

	vfs_builder.MakeWritable();

	if (!writable)
		vfs_builder.ScheduleRemount(MS_RDONLY, 0);
}

[[gnu::pure]]
static std::string_view
DirName(const char *path) noexcept
{
	const char *slash = strrchr(path, '/');
	if (slash == nullptr)
		return {};

	return {path, std::size_t(slash - path)};
}

static const char *
WriteToTempFile(char *buffer, std::span<const std::byte> contents)
{
	unsigned long n = time(nullptr);

	while (true) {
		sprintf(buffer, "/tmp/%lx", n);

		UniqueFileDescriptor fd;
		if (fd.Open(buffer, O_CREAT|O_EXCL|O_WRONLY, 0644)) {
			fd.Write(contents);
			return buffer;
		}

		switch (const int e = errno) {
		case EEXIST:
			/* try again with new name */
			++n;
			continue;

		default:
			throw MakeErrno(e, "Failed to create file");
		}
	}
}

[[gnu::pure]]
static bool
PathExists(const char *path) noexcept
{
	struct stat st;
	return lstat(path, &st) == 0;
}

inline void
Mount::ApplyWriteFile(VfsBuilder &vfs_builder) const
{
	assert(type == Type::WRITE_FILE);
	assert(source != nullptr);
	assert(target != nullptr);

	const auto contents = AsBytes(std::string_view{source});

	if (const auto dir = DirName(target);
	    vfs_builder.MakeOptionalDirectory(dir)) {
		/* inside a tmpfs: create the file here */
		auto fd = OpenWriteOnly(target, O_CREAT|O_TRUNC);
		fd.Write(contents);
	} else {
		/* inside a read-only mount: create the file in /tmp
		   and bind-mount it over the existing (read-only)
		   file */

		if (optional && !PathExists(target))
			return;

		char buffer[64];
		const char *tmp_path = WriteToTempFile(buffer, contents);

		constexpr uint_least64_t attr_set = MS_NOSUID|MS_NODEV|MS_RDONLY|MS_NOEXEC;
		BindMount(tmp_path, target);
		MountSetAttr(FileDescriptor::Undefined(), target,
			     AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT, attr_set, 0);
	}
}

inline void
Mount::Apply(VfsBuilder &vfs_builder) const
{
	switch (type) {
	case Type::BIND:
		ApplyBindMount(vfs_builder);
		break;

	case Type::BIND_FILE:
		ApplyBindMountFile(vfs_builder);
		break;

	case Type::TMPFS:
		ApplyTmpfs(vfs_builder);
		break;

	case Type::NAMED_TMPFS:
		ApplyNamedTmpfs(vfs_builder);
		break;

	case Type::WRITE_FILE:
		ApplyWriteFile(vfs_builder);
		break;
	}
}

void
Mount::ApplyAll(const IntrusiveForwardList<Mount> &m,
		VfsBuilder &vfs_builder)
{
	for (const auto &i : m)
		i.Apply(vfs_builder);
}

char *
Mount::MakeId(char *p) const noexcept
{
	switch (type) {
	case Type::BIND:
		*p++ = ';';
		*p++ = 'm';
		break;

	case Type::BIND_FILE:
		*p++ = ';';
		*p++ = 'f';
		break;

	case Type::TMPFS:
		p = (char *)mempcpy(p, ";t:", 3);
		p = stpcpy(p, target);
		return p;

	case Type::NAMED_TMPFS:
		p = (char *)mempcpy(p, ";nt:", 4);
		p = stpcpy(p, source);
		*p++ = '>';
		p = stpcpy(p, target);
		return p;

	case Type::WRITE_FILE:
		p = (char *)mempcpy(p, ";wf:", 4);
		p = stpcpy(p, target);
		*p++ = '=';
		p = stpcpy(p, source);
		*p++ = ';';
		return p;
	}

	if (writable)
		*p++ = 'w';

	if (exec)
		*p++ = 'x';

	*p++ = ':';
	p = stpcpy(p, source);
	*p++ = '>';
	p = stpcpy(p, target);

	return p;
}

char *
Mount::MakeIdAll(char *p, const IntrusiveForwardList<Mount> &m) noexcept
{
	for (const auto &i : m)
		p = i.MakeId(p);

	return p;
}
