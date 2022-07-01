/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "Mount.hxx"
#include "VfsBuilder.hxx"
#include "system/BindMount.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringSplit.hxx"
#include "AllocatorPtr.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <cinttypes>

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

static void
MountOrThrow(const char *source, const char *target,
	     const char *filesystemtype, unsigned long mountflags,
	     const void *data)
{
	if (mount(source, target, filesystemtype, mountflags, data) < 0)
		throw FormatErrno("mount('%s') failed", target);
}

inline void
Mount::ApplyBindMount(VfsBuilder &vfs_builder) const
{
	if (struct stat st;
	    optional && lstat(source, &st) < 0 && errno == ENOENT)
		/* the source directory doesn't exist, but this is
		   optional, so just ignore it */
		return;

	vfs_builder.Add(target);

	int flags = MS_NOSUID|MS_NODEV;
	if (!writable)
		flags |= MS_RDONLY;
	if (!exec)
		flags |= MS_NOEXEC;

	BindMount(source, target, flags);
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
		const auto parent = Split(std::string_view{target}, '/').first;
		vfs_builder.MakeDirectory(parent);

		UniqueFileDescriptor fd;
		if (!fd.Open(target, O_CREAT|O_WRONLY, 0666))
			throw FormatErrno("Failed to create %s", target);
	}

	constexpr int flags = MS_NOSUID|MS_NODEV|MS_RDONLY|MS_NOEXEC;
	BindMount(source, target, flags);
}

inline void
Mount::ApplyTmpfs(VfsBuilder &vfs_builder) const
{
	vfs_builder.Add(target);

	int flags = MS_NOSUID|MS_NODEV;
	if (!exec)
		flags |= MS_NOEXEC;

	const char *options = "size=16M,nr_inodes=256,mode=711";
	char options_buffer[64];
	if (writable) {
		snprintf(options_buffer, sizeof(options_buffer),
			 "%s,uid=%" PRIuLEAST32 ",gid=%" PRIuLEAST32,
			 options, vfs_builder.uid, vfs_builder.gid);
		options = options_buffer;
	}

	MountOrThrow("none", target, "tmpfs", flags,
		     options);

	vfs_builder.MakeWritable();

	if (!writable)
		vfs_builder.ScheduleRemount(flags | MS_RDONLY);
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
