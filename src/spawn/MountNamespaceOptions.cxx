// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MountNamespaceOptions.hxx"
#include "Mount.hxx"
#include "VfsBuilder.hxx"
#include "UidGid.hxx"
#include "AllocatorPtr.hxx"
#include "system/pivot_root.h"
#include "system/BindMount.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringAPI.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>

#ifndef __linux__
#error This library requires Linux
#endif

MountNamespaceOptions::MountNamespaceOptions(AllocatorPtr alloc,
					     const MountNamespaceOptions &src) noexcept
	:mount_root_tmpfs(src.mount_root_tmpfs),
	 mount_proc(src.mount_proc),
	 writable_proc(src.writable_proc),
	 mount_dev(src.mount_dev),
	 mount_pts(src.mount_pts),
	 bind_mount_pts(src.bind_mount_pts),
#if TRANSLATION_ENABLE_EXPAND
	 expand_home(src.expand_home),
#endif
	 pivot_root(alloc.CheckDup(src.pivot_root)),
	 home(alloc.CheckDup(src.home)),
	 mount_tmp_tmpfs(alloc.CheckDup(src.mount_tmp_tmpfs)),
	 mounts(Mount::CloneAll(alloc, src.mounts))
{
}

#if TRANSLATION_ENABLE_EXPAND

bool
MountNamespaceOptions::IsExpandable() const noexcept
{
	return expand_home || Mount::IsAnyExpandable(mounts);
}

void
MountNamespaceOptions::Expand(AllocatorPtr alloc, const MatchData &match_data)
{
	if (expand_home) {
		expand_home = false;
		home = expand_string_unescaped(alloc, home, match_data);
	}

	Mount::ExpandAll(alloc, mounts, match_data);
}

#endif

static void
ChdirOrThrow(const char *path)
{
	if (chdir(path) < 0)
		throw FormatErrno("chdir('%s') failed", path);
}

static void
MountOrThrow(const char *source, const char *target,
	     const char *filesystemtype, unsigned long mountflags,
	     const void *data)
{
	if (mount(source, target, filesystemtype, mountflags, data) < 0)
		throw FormatErrno("mount('%s') failed", target);
}

void
MountNamespaceOptions::Apply(const UidGid &uid_gid) const
{
	if (!IsEnabled())
		return;

	/* convert all "shared" mounts to "private" mounts */
	mount(nullptr, "/", nullptr, MS_PRIVATE|MS_REC, nullptr);

	const char *const put_old = "/mnt";

	const char *new_root = nullptr;

	VfsBuilder vfs_builder(uid_gid.uid, uid_gid.gid);

	if (pivot_root != nullptr) {
		/* first bind-mount the new root onto itself to "unlock" the
		   kernel's mount object (flag MNT_LOCKED) in our namespace;
		   without this, the kernel would not allow an unprivileged
		   process to pivot_root to it */

		new_root = pivot_root;
		BindMount(new_root, new_root, MS_NOSUID|MS_RDONLY);

		/* release a reference to the old root */
		ChdirOrThrow(new_root);
	} else if (mount_root_tmpfs) {
		new_root = "/tmp";

		/* create an empty tmpfs as the new filesystem root */
		MountOrThrow("none", new_root, "tmpfs", MS_NODEV|MS_NOEXEC|MS_NOSUID,
			     "size=256k,nr_inodes=1024,mode=755");

		ChdirOrThrow(new_root);

		vfs_builder.AddWritableRoot(new_root);
		vfs_builder.Add(put_old);
	}

	if (new_root != nullptr) {
		/* enter the new root */
		int result = my_pivot_root(new_root, put_old + 1);
		if (result < 0)
			throw FormatErrno("pivot_root('%s') failed", new_root);
	}

	if (mount_proc) {
		if (new_root == nullptr)
			/* if we're still in the old filesystem root
			   (no pivot_root()), /proc is already
			   mounted, so we need to unmount it first to
			   allow mounting a new /proc instance, or
			   else that will fail with EBUSY */
			umount2("/proc", MNT_DETACH);

		vfs_builder.Add("/proc");

		unsigned long flags = MS_NOEXEC|MS_NOSUID|MS_NODEV;
		if (!writable_proc)
			flags |= MS_RDONLY;

		MountOrThrow("proc", "/proc", "proc", flags, nullptr);
	}

	if (mount_dev) {
		vfs_builder.Add("/dev");

		ChdirOrThrow(new_root != nullptr ? put_old : "/");

		// TODO no bind-mount, just create /dev/null etc.
		const char *source = "dev";
		const char *target = "/dev";
		if (mount(source, target, nullptr, MS_BIND|MS_REC, nullptr) < 0)
			throw FormatErrno("bind_mount('%s', '%s') failed", source, target);

		if (new_root != nullptr)
			/* back to the new root */
			ChdirOrThrow("/");
	}

	if (mount_pts) {
		vfs_builder.Add("/dev/pts");

		/* the "newinstance" option is only needed with pre-4.7
		   kernels; from v4.7 on, this is implicit for all new devpts
		   mounts (kernel commit eedf265aa003) */
		MountOrThrow("devpts", "/dev/pts", "devpts",
			     MS_NOEXEC|MS_NOSUID,
			     "newinstance");
	}

	if (mount_tmp_tmpfs != nullptr) {
		const char *options = "size=16M,nr_inodes=256,mode=1777";
		char buffer[256];
		if (*mount_tmp_tmpfs != 0) {
			snprintf(buffer, sizeof(buffer), "%s,%s", options, mount_tmp_tmpfs);
			options = buffer;
		}

		vfs_builder.Add("/tmp");

		MountOrThrow("none", "/tmp", "tmpfs",
			     MS_NODEV|MS_NOEXEC|MS_NOSUID,
			     options);

		vfs_builder.MakeWritable();
	}

	if (HasBindMount()) {
		/* go to /mnt so we can refer to the old directories with a
		   relative path */
		ChdirOrThrow(new_root != nullptr ? put_old : "/");

		if (bind_mount_pts) {
			vfs_builder.Add("/dev/pts");
			BindMount("dev/pts", "/dev/pts", MS_NOSUID|MS_NOEXEC);
		}

		Mount::ApplyAll(mounts, vfs_builder);

		if (new_root != nullptr)
			/* back to the new root */
			ChdirOrThrow("/");
	}

	if (new_root != nullptr &&
	    /* get rid of the old root */
	    umount2(put_old, MNT_DETACH) < 0)
		throw FormatErrno("umount('%s') failed", put_old);

	if (mount_root_tmpfs) {
		rmdir(put_old);

		/* make the root tmpfs read-only */

		if (mount(nullptr, "/", nullptr,
			  MS_REMOUNT|MS_BIND|MS_NODEV|MS_NOEXEC|MS_NOSUID|MS_RDONLY,
			  nullptr) < 0)
			throw MakeErrno("Failed to remount read-only");
	}

	vfs_builder.Finish();
}

char *
MountNamespaceOptions::MakeId(char *p) const noexcept
{
	p = (char *)(char *)mempcpy(p, ";mns", 4);

	if (pivot_root != nullptr) {
		p = (char *)mempcpy(p, ";pvr=", 5);
		p = stpcpy(p, pivot_root);
	}

	if (mount_root_tmpfs)
		p = (char *)mempcpy(p, ";rt", 3);

	if (mount_proc) {
		p = (char *)mempcpy(p, ";proc", 5);
		if (writable_proc)
			*p++ = 'w';
	}

	if (mount_dev)
		p = (char *)mempcpy(p, ";dev", 4);

	if (mount_pts)
		p = (char *)mempcpy(p, ";pts", 4);

	if (bind_mount_pts)
		p = (char *)mempcpy(p, ";bpts", 4);

	if (mount_tmp_tmpfs != nullptr) {
		p = (char *)mempcpy(p, ";tt:", 3);
		p = stpcpy(p, mount_tmp_tmpfs);
	}

	p = Mount::MakeIdAll(p, mounts);

	return p;
}

const Mount *
MountNamespaceOptions::FindMountHome() const noexcept
{
	assert(home != nullptr);
	assert(*home == '/');

	/* skip the leading slash which is also skipped in
	   Mount::Source */
	const char *const source = home + 1;

	for (const auto &i : mounts)
		if (i.type == Mount::Type::BIND &&
		    StringIsEqual(i.source, source))
			return &i;

	return nullptr;
}

const char *
MountNamespaceOptions::GetMountHome() const noexcept
{
	if (const auto *m = FindMountHome())
		return m->target;
	else
		return nullptr;
}

const char *
MountNamespaceOptions::GetJailedHome() const noexcept
{
	if (const auto *m = FindMountHome())
		return m->target;

	return home;
}
