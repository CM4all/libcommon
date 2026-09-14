// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Mount.hxx"
#include "MakeId.hxx"
#include "TmpfsCreate.hxx"
#include "VfsBuilder.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "system/linux/Mount.hxx"
#include "system/linux/openat2.h"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/SpanCast.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "AllocatorPtr.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <fmt/format.h>

#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <time.h> // for time()

using std::string_view_literals::operator""sv;

[[gnu::pure]]
static std::string_view
DirName(const char *path) noexcept
{
	const char *slash = strrchr(path, '/');
	if (slash == nullptr)
		return {};

	return {path, std::size_t(slash - path)};
}

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

bool
Mount::IsSourcePath(const char *path) const noexcept
{
	assert(path != nullptr);
	assert(*path == '/');
	assert(source != nullptr);

	/* skip the leading slash which is also skipped in source */
	++path;

	return StringIsEqual(source, path);
}

const char *
Mount::IsInSourcePath(const char *path) const noexcept
{
	assert(path != nullptr);
	assert(*path == '/');
	assert(source != nullptr);

	/* skip the leading slash which is also skipped in source */
	++path;

	const char *rest = StringAfterPrefix(path, source);
	if (rest != nullptr && (*rest != '/' && *rest != '\0'))
		rest = nullptr;

	return rest;
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
OpenDirectoryPathNoSymlinks(FileDescriptor directory, const char *path)
{
	static constexpr struct open_how how{
		.flags = O_PATH|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,
		.resolve = RESOLVE_IN_ROOT|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_SYMLINKS,
	};

	int fd = openat2(directory.Get(), path, &how, sizeof(how));
	if (fd < 0)
		throw FmtErrno("Failed to open {:?}", path);

	return UniqueFileDescriptor{AdoptTag{}, fd};
}

static UniqueFileDescriptor
OpenTreeNoSymlinks(FileDescriptor directory, const char *path)
{
	return OpenTree({OpenDirectoryPathNoSymlinks(directory, path), ""},
			AT_EMPTY_PATH|OPEN_TREE_CLONE);
}

inline void
Mount::ApplyBindMount(VfsBuilder &vfs_builder, FileDescriptor root_fd,
		      FileDescriptor old_root_fd) const
{
	if (struct statx st;
	    optional && !source_fd.IsDefined() &&
	    statx(old_root_fd.Get(), source, AT_SYMLINK_NOFOLLOW|AT_STATX_DONT_SYNC,
		  STATX_TYPE, &st) < 0 && errno == ENOENT)
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

	UniqueFileDescriptor ufd;
	FileAt source_at{source_fd, ""};
	if (!source_fd.IsDefined())
		source_at.directory = ufd = OpenTreeNoSymlinks(old_root_fd, source);

	MountSetAttr(source_at,
		     AT_EMPTY_PATH,
		     attr_set, attr_clr);

	MoveMount(source_at,
		  {root_fd, target + 1},
		  MOVE_MOUNT_F_EMPTY_PATH);
}

inline void
Mount::ApplyBindMountFile(VfsBuilder &vfs_builder, FileDescriptor root_fd,
			  FileDescriptor old_root_fd) const
{
	if (struct statx st;
	    optional && !source_fd.IsDefined() &&
	    statx(old_root_fd.Get(), source, AT_SYMLINK_NOFOLLOW|AT_STATX_DONT_SYNC,
		  STATX_TYPE, &st) < 0 && errno == ENOENT)
		/* the source file doesn't exist, but this is
		   optional, so just ignore it */
		return;

	if (struct statx st;
	    optional && !source_fd.IsDefined() &&
	    statx(root_fd.Get(), target + 1, AT_SYMLINK_NOFOLLOW|AT_STATX_DONT_SYNC,
		  STATX_TYPE, &st) == 0) {
		/* target exists already */
		if (!S_ISREG(st.stx_mode))
			throw FmtRuntimeError("Not a regular file: {:?}"sv,
					      target);
	} else if (const int e = errno; e != ENOENT) {
		throw FmtErrno(e, "Failed to stat {:?}"sv, target);
	} else {
		/* target does not exist: first ensure that its parent
		   directory exists, then create an empty target */
		vfs_builder.MakeDirectory(DirName(target));

		UniqueFileDescriptor fd;
		if (!fd.Open(target, O_CREAT|O_EXCL|O_WRONLY, 0666))
			throw FmtErrno("Failed to create {:?}"sv, target);
	}

	uint_least64_t attr_set = MS_NOSUID|MS_NODEV|MS_RDONLY, attr_clr = 0;
	if (exec)
		attr_clr |= MS_NOEXEC;
	else
		attr_set |= MS_NOEXEC;

	UniqueFileDescriptor ufd;
	FileAt source_at{source_fd, ""};
	if (!source_fd.IsDefined())
		source_at.directory = ufd = OpenTreeNoSymlinks(old_root_fd, source);

	MountSetAttr(source_at,
		     AT_EMPTY_PATH,
		     attr_set, attr_clr);

	MoveMount(source_at,
		  {root_fd, target + 1},
		  MOVE_MOUNT_F_EMPTY_PATH);
}

inline void
Mount::ApplyTmpfs(VfsBuilder &vfs_builder, FileDescriptor root_fd) const
{
	vfs_builder.Add(target);

	int flags = MS_NOSUID|MS_NODEV;
	if (!exec)
		flags |= MS_NOEXEC;

	auto fs = FSOpen("tmpfs");
	FSConfig(fs, FSCONFIG_SET_STRING, "size", "16M");
	FSConfig(fs, FSCONFIG_SET_STRING, "nr_inodes", "256");
	FSConfig(fs, FSCONFIG_SET_STRING, "mode", FmtBuffer<8>("{:o}", vfs_builder.GetDirMode()));

	if (writable) {
		FSConfig(fs, FSCONFIG_SET_STRING, "uid",
			 fmt::format_int(vfs_builder.uid).c_str());
		FSConfig(fs, FSCONFIG_SET_STRING, "gid",
			 fmt::format_int(vfs_builder.gid).c_str());
	}

	FSConfig(fs, FSCONFIG_CMD_CREATE, nullptr, nullptr);

	MoveMount({FSMount(fs, flags), ""},
		  {root_fd, target + 1},
		  MOVE_MOUNT_F_EMPTY_PATH);

	vfs_builder.MakeWritable(root_fd);

	if (!writable)
		vfs_builder.ScheduleRemount(MS_RDONLY, 0);
}

inline void
Mount::ApplyNamedTmpfs(VfsBuilder &vfs_builder, FileDescriptor root_fd) const
{
	vfs_builder.Add(target);

	if (source_fd.IsDefined()) {
		MoveMount({source_fd, ""},
			  {root_fd, target + 1},
			  MOVE_MOUNT_F_EMPTY_PATH);
	} else {
		/* we didn't get a "source_fd", so just create a new
		   one (which will not be shared with anybody, just a
		   fallback) */

		MoveMount({CreateTmpfs(exec), ""},
			  {root_fd, target + 1},
			  MOVE_MOUNT_F_EMPTY_PATH);
	}

	vfs_builder.MakeWritable(root_fd);

	if (!writable)
		vfs_builder.ScheduleRemount(MS_RDONLY, 0);
}

static UniqueFileDescriptor
WriteToTempFile(std::span<const std::byte> contents)
{
	unsigned long n = time(nullptr);

	while (true) {
		char buffer[64];
		sprintf(buffer, "/tmp/%lx", n);

		UniqueFileDescriptor fd;
		if (fd.Open(buffer, O_CREAT|O_EXCL|O_WRONLY, 0644)) {
			if (fd.Write(contents) < 0)
				throw MakeErrno("Failed to write");

			return fd;
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
PathExists(FileAt file) noexcept
{
	struct statx st;
	return statx(file.directory.Get(), file.name, AT_SYMLINK_NOFOLLOW|AT_STATX_DONT_SYNC,
		     STATX_TYPE, &st) == 0;
}

inline void
Mount::ApplyWriteFile(VfsBuilder &vfs_builder, FileDescriptor root_fd) const
{
	assert(type == Type::WRITE_FILE);
	assert(source != nullptr);
	assert(target != nullptr);

	const auto contents = AsBytes(std::string_view{source});

	if (const auto dir = DirName(target);
	    vfs_builder.MakeOptionalDirectory(dir)) {
		/* inside a tmpfs: create the file here */
		auto fd = OpenWriteOnly({root_fd, target + 1}, O_CREAT|O_TRUNC);

		if (fd.Write(contents) < 0)
			throw MakeErrno("Failed to write");
	} else {
		/* inside a read-only mount: create the file in /tmp
		   and bind-mount it over the existing (read-only)
		   file */

		if (optional && !PathExists({root_fd, target + 1}))
			return;

		const auto fd = OpenTree({WriteToTempFile(contents), ""},
					 AT_EMPTY_PATH|OPEN_TREE_CLONE);

		constexpr uint_least64_t attr_set = MS_NOSUID|MS_NODEV|MS_RDONLY|MS_NOEXEC;
		MountSetAttr({fd, ""}, AT_EMPTY_PATH, attr_set, 0);
		MoveMount({fd, ""}, {root_fd, target + 1}, MOVE_MOUNT_F_EMPTY_PATH);
	}
}

inline void
Mount::ApplySymlink(VfsBuilder &vfs_builder, FileDescriptor root_fd) const
{
	assert(type == Type::SYMLINK);
	assert(source != nullptr);
	assert(target != nullptr);

	vfs_builder.MakeDirectory(DirName(target));

	if (symlinkat(source, root_fd.Get(), target + 1) < 0)
		throw FmtErrno("Failed to create symlink {:?}", target);
}

inline void
Mount::Apply(VfsBuilder &vfs_builder, FileDescriptor root_fd, FileDescriptor old_root_fd) const
{
	switch (type) {
	case Type::BIND:
		ApplyBindMount(vfs_builder, root_fd, old_root_fd);
		break;

	case Type::BIND_FILE:
		ApplyBindMountFile(vfs_builder, root_fd, old_root_fd);
		break;

	case Type::TMPFS:
		ApplyTmpfs(vfs_builder, root_fd);
		break;

	case Type::NAMED_TMPFS:
		ApplyNamedTmpfs(vfs_builder, root_fd);
		break;

	case Type::WRITE_FILE:
		ApplyWriteFile(vfs_builder, root_fd);
		break;

	case Type::SYMLINK:
		ApplySymlink(vfs_builder, root_fd);
		break;
	}
}

void
Mount::ApplyAll(const IntrusiveForwardList<Mount> &m,
		VfsBuilder &vfs_builder, FileDescriptor root_fd,
		FileDescriptor old_root_fd)
{
	for (const auto &i : m)
		i.Apply(vfs_builder, root_fd, old_root_fd);
}

char *
Mount::MakeId(char *p) const noexcept
{
	switch (type) {
	case Type::BIND:
		p = AppendString(p, ";m"sv);
		break;

	case Type::BIND_FILE:
		p = AppendString(p, ";f"sv);
		break;

	case Type::TMPFS:
		p = AppendValue(p, ";t:"sv, target);
		return p;

	case Type::NAMED_TMPFS:
		p = AppendValue(p, ";nt:"sv, source);
		p = AppendValue(p, ">"sv, target);
		return p;

	case Type::WRITE_FILE:
		p = AppendValue(p, ";wf:"sv, target);
		p = AppendValue(p, "="sv, source);
		*p++ = ';';
		return p;

	case Type::SYMLINK:
		p = AppendValue(p, ";sy:"sv, target);
		p = AppendValue(p, ">"sv, source);
		*p++ = ';';
		return p;
	}

	p = AppendOptional(p, 'w', writable);
	p = AppendOptional(p, 'x', exec);

	p = AppendValue(p, ":"sv, source);
	p = AppendValue(p, ">"sv, target);

	return p;
}

char *
Mount::MakeIdAll(char *p, const IntrusiveForwardList<Mount> &m) noexcept
{
	for (const auto &i : m)
		p = i.MakeId(p);

	return p;
}
