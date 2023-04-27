// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Mount.hxx"
#include "VfsBuilder.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "system/Mount.hxx"
#include "system/Error.hxx"
#include "system/Mount.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/RuntimeError.hxx"
#include "util/SpanCast.hxx"
#include "util/StringSplit.hxx"
#include "AllocatorPtr.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

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

inline void
Mount::ApplyBindMount(VfsBuilder &vfs_builder) const
{
	if (struct stat st;
	    optional && lstat(source, &st) < 0 && errno == ENOENT)
		/* the source directory doesn't exist, but this is
		   optional, so just ignore it */
		return;

	vfs_builder.Add(target);

	uint_least64_t flags = MS_NOSUID|MS_NODEV;
	if (!writable)
		flags |= MS_RDONLY;
	if (!exec)
		flags |= MS_NOEXEC;

	BindMount(source, target);
	MountSetAttr(FileDescriptor::Undefined(), target,
		     AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT, flags, 0);
}

inline void
Mount::ApplyBindMountFile(VfsBuilder &vfs_builder) const
{
	if (struct stat st;
	    optional && lstat(source, &st) < 0 && errno == ENOENT)
		/* the source file doesn't exist, but this is
		   optional, so just ignore it */
		return;

	if (struct stat st; lstat(target, &st) == 0) {
		/* target exists already */
		if (!S_ISREG(st.st_mode))
			throw FormatRuntimeError("Not a regular file: %s",
						 target);
	} else if (const int e = errno; e != ENOENT) {
		throw FormatErrno(e, "Failed to stat %s",
				  target);
	} else {
		/* target does not exist: first ensure that its parent
		   directory exists, then create an empty target */
		const auto parent = SplitLast(std::string_view{target}, '/').first;
		vfs_builder.MakeDirectory(parent);

		UniqueFileDescriptor fd;
		if (!fd.Open(target, O_CREAT|O_WRONLY, 0666))
			throw FormatErrno("Failed to create %s", target);
	}

	constexpr uint_least64_t flags = MS_NOSUID|MS_NODEV|MS_RDONLY|MS_NOEXEC;
	BindMount(source, target);
	MountSetAttr(FileDescriptor::Undefined(), target,
		     AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT, flags, 0);
}

inline void
Mount::ApplyTmpfs(VfsBuilder &vfs_builder) const
{
	vfs_builder.Add(target);

	int flags = MS_NOSUID|MS_NODEV;
	if (!exec)
		flags |= MS_NOEXEC;

	const char *options = "size=16M,nr_inodes=256,mode=711";
	StringBuffer<64> options_buffer;
	if (writable) {
		options_buffer = FmtBuffer<64>("{},uid={},gid={}",
					       options, vfs_builder.uid, vfs_builder.gid);
		options = options_buffer;
	}

	MountOrThrow("none", target, "tmpfs", flags,
		     options);

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
			fd.Write(contents.data(), contents.size());
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
		fd.Write(contents.data(), contents.size());
	} else {
		/* inside a read-only mount: create the file in /tmp
		   and bind-mount it over the existing (read-only)
		   file */

		if (optional && !PathExists(target))
			return;

		char buffer[64];
		const char *tmp_path = WriteToTempFile(buffer, contents);

		constexpr uint_least64_t flags = MS_NOSUID|MS_NODEV|MS_RDONLY|MS_NOEXEC;
		BindMount(tmp_path, target);
		MountSetAttr(FileDescriptor::Undefined(), target,
			     AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT, flags, 0);
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
