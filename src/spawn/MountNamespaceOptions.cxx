// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "MountNamespaceOptions.hxx"
#include "MakeId.hxx"
#include "Mount.hxx"
#include "VfsBuilder.hxx"
#include "UidGid.hxx"
#include "AllocatorPtr.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "system/linux/pivot_root.h"
#include "system/linux/Mount.hxx"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/IterableSplitString.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringAPI.hxx"
#include "util/StringSplit.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <algorithm>

#include <assert.h>
#include <fcntl.h> // for AT_*
#include <unistd.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>

using std::string_view_literals::operator""sv;

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
	 mount_listen_stream(alloc.Dup(src.mount_listen_stream)),
	 mounts(Mount::CloneAll(alloc, src.mounts)),
	 dir_mode(src.dir_mode),
	 mount_tmp_tmpfs_exec(src.mount_tmp_tmpfs_exec)
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
ChdirOrThrow(FileDescriptor fd)
{
	if (fchdir(fd.Get()) < 0)
		throw MakeErrno("fchdir() failed");
}

/**
 * Split a comma-separated string of mount options and feed it into
 * fsconfig().  Throws on error.
 */
static void
FSConfigSplit(FileDescriptor fd, std::string_view options)
{
	for (const std::string_view i : IterableSplitString(options, ',')) {
		const auto [name, value] = Split(i, '=');
		FSConfig(fd, FSCONFIG_SET_STRING,
			 std::string{name}.c_str(), std::string{value}.c_str());
	}
}

inline UniqueFileDescriptor
MountNamespaceOptions::OpenRootMount() const
{
	if (pivot_root != nullptr) {
		auto fd = OpenTree({FileDescriptor::Undefined(), pivot_root},
				   AT_SYMLINK_NOFOLLOW|OPEN_TREE_CLONE);

		/* make it read-only and nosuid, but allow executables
		   and device nodes */
		MountSetAttr({fd, ""},
			     AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT,
			     MS_NOSUID|MS_RDONLY,
			     MS_NOEXEC|MS_NODEV);

		return fd;
	} else if (mount_root_tmpfs) {
		/* create an empty tmpfs as the new filesystem root */
		auto fs = FSOpen("tmpfs");
		FSConfig(fs, FSCONFIG_SET_STRING, "size", "256k");
		FSConfig(fs, FSCONFIG_SET_STRING, "nr_inodes", "1024");
		FSConfig(fs, FSCONFIG_SET_STRING, "mode", "755");
		FSConfig(fs, FSCONFIG_CMD_CREATE, nullptr, nullptr);

		return FSMount(fs, MS_NODEV|MS_NOEXEC|MS_NOSUID);
	} else
		/* no new root; open the original root mount
		   recursively */
		return OpenTree({FileDescriptor::Undefined(), "/"},
				AT_SYMLINK_NOFOLLOW|AT_RECURSIVE|OPEN_TREE_CLONE);
}

void
MountNamespaceOptions::Apply(const UidGid &uid_gid) const
{
	if (!IsEnabled())
		return;

	/* convert all "shared" mounts to "private" mounts */
	MountSetAttr({FileDescriptor::Undefined(), "/"},
		     AT_RECURSIVE|AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT,
		     0, 0, MS_PRIVATE);

	const auto old_root_fd = OpenDirectoryPath({FileDescriptor::Undefined(), "/"});

	const char *const put_old = "/mnt";

	const char *new_root = nullptr;

	VfsBuilder vfs_builder{uid_gid.effective_uid, uid_gid.effective_gid, dir_mode};

	auto root_fd = OpenRootMount();

	/* release a reference to the old root */
	ChdirOrThrow(root_fd);

	bool have_proc = false;
	if (pivot_root != nullptr) {
		/* first bind-mount the new root onto itself to "unlock" the
		   kernel's mount object (flag MNT_LOCKED) in our namespace;
		   without this, the kernel would not allow an unprivileged
		   process to pivot_root to it */

		new_root = pivot_root;
	} else if (mount_root_tmpfs) {
		new_root = "/tmp";

		vfs_builder.AddWritableRoot(root_fd);
		vfs_builder.ScheduleRemount(MS_RDONLY, 0);

		vfs_builder.Add(put_old);
	} else {
		new_root = "/tmp";
		have_proc = true;
	}

	if (mount_proc) {
		if (have_proc)
			/* if we're still in the old filesystem root
			   (no pivot_root()), /proc is already
			   mounted, so we need to unmount it first to
			   allow mounting a new /proc instance, or
			   else that will fail with EBUSY */
			umount2("proc", MNT_DETACH);

		vfs_builder.Add("/proc");

		unsigned long flags = MS_NOEXEC|MS_NOSUID|MS_NODEV;
		if (!writable_proc)
			flags |= MS_RDONLY;

		const auto fs = FSOpen("proc");
		FSConfig(fs, FSCONFIG_SET_STRING, "hidepid", "1");
		FSConfig(fs, FSCONFIG_SET_STRING, "subset", "pid");
		FSConfig(fs, FSCONFIG_CMD_CREATE, nullptr, nullptr);
		MoveMount({FSMount(fs, flags), ""},
			  {root_fd, "proc"},
			  MOVE_MOUNT_F_EMPTY_PATH);
	}

	if (mount_dev) {
		vfs_builder.Add("/dev");

		// TODO no bind-mount, just create /dev/null etc.
		MoveMount({OpenTree({old_root_fd, "dev"},
				    AT_SYMLINK_NOFOLLOW|AT_RECURSIVE|OPEN_TREE_CLONE), ""},
			{root_fd, "dev"},
			MOVE_MOUNT_F_EMPTY_PATH);
	}

	if (mount_pts) {
		vfs_builder.Add("/dev/pts");

		const auto fs = FSOpen("devpts");
		FSConfig(fs, FSCONFIG_CMD_CREATE, nullptr, nullptr);

		MoveMount({FSMount(fs, MS_NOEXEC|MS_NOSUID), ""},
			  {root_fd, "dev/pts"},
			  MOVE_MOUNT_F_EMPTY_PATH);
	}

	if (mount_tmp_tmpfs != nullptr) {
		vfs_builder.Add("/tmp");

		unsigned long flags = MS_NODEV|MS_NOSUID;
		if (!mount_tmp_tmpfs_exec)
			flags |= MS_NOEXEC;

		auto fs = FSOpen("tmpfs");
		FSConfig(fs, FSCONFIG_SET_STRING, "size", "16M");
		FSConfig(fs, FSCONFIG_SET_STRING, "nr_inodes", "256");
		FSConfig(fs, FSCONFIG_SET_STRING, "mode", "1777");

		if (*mount_tmp_tmpfs != '\0')
			FSConfigSplit(fs, mount_tmp_tmpfs);

		FSConfig(fs, FSCONFIG_CMD_CREATE, nullptr, nullptr);

		MoveMount({FSMount(fs, flags), ""},
			  {root_fd, "tmp"},
			  MOVE_MOUNT_F_EMPTY_PATH);

		vfs_builder.MakeWritable(root_fd);
	}

	if (HasBindMount()) {
		if (bind_mount_pts) {
			vfs_builder.Add("/dev/pts");
			MoveMount({OpenTree({old_root_fd, "dev/pts"},
					    AT_SYMLINK_NOFOLLOW|OPEN_TREE_CLONE), ""},
				{root_fd, "dev/pts"},
				MOVE_MOUNT_F_EMPTY_PATH);
		}

		Mount::ApplyAll(mounts, vfs_builder, root_fd, old_root_fd);
	}

	MoveMount({root_fd, ""},
		  {FileDescriptor::Undefined(), new_root},
		  MOVE_MOUNT_F_EMPTY_PATH);

	if (new_root != nullptr) {
		/* enter the new root */
		int result = my_pivot_root(new_root, put_old + 1);
		if (result < 0)
			throw FmtErrno("pivot_root({:?}) failed", new_root);

		/* get rid of the old root */
		Umount(put_old, MNT_DETACH);
	}

	if (mount_root_tmpfs) {
		rmdir(put_old);
	}

	vfs_builder.Finish();
}

char *
MountNamespaceOptions::MakeId(char *p) const noexcept
{
	p = AppendString(p, ";mns"sv);
	p = AppendOptionalValue(p, ";pvr="sv, pivot_root);
	p = AppendOptional(p, ";rt"sv, mount_root_tmpfs);

	if (mount_proc) {
		p = AppendString(p, ";proc"sv);
		p = AppendOptional(p, 'w', writable_proc);
	}

	p = AppendOptional(p, ";dev"sv, mount_dev);
	p = AppendOptional(p, ";pts"sv, mount_pts);
	p = AppendOptional(p, ";bpts"sv, bind_mount_pts);
	p = AppendOptionalValue(p, ";tt:"sv, mount_tmp_tmpfs);
	p = AppendOptionalDjbHash(p, ";ls"sv, mount_listen_stream);

	p = Mount::MakeIdAll(p, mounts);

	return p;
}

bool
MountNamespaceOptions::HasMountOn(const char *target) const noexcept
{
	assert(target != nullptr);
	assert(*target == '/');

	return std::any_of(mounts.begin(), mounts.end(), [target](const auto &i){
		return i.type == Mount::Type::BIND &&
		       StringIsEqual(i.target, target);
	});
}

std::pair<const Mount *, const char *>
MountNamespaceOptions::FindBindMountInSource(const char *host_path) const noexcept
{
	assert(host_path != nullptr);
	assert(*host_path == '/');

	for (const auto &i : mounts) {
		if (i.type == Mount::Type::BIND) {
			const char *rest = i.IsInSourcePath(host_path);
			if (rest != nullptr)
				return {&i, rest};
		}
	}

	return {};
}

const char *
MountNamespaceOptions::ToContainerPath(AllocatorPtr alloc,
				       const char *host_path) const noexcept
{
	if (!IsRootMounted())
		/* no translation needed */
		return host_path;

	const auto [mount, rest] = FindBindMountInSource(host_path);
	if (mount == nullptr)
		return nullptr;

	if (*rest == '\0')
		return mount->target;

	return alloc.Concat(mount->target, rest);
}
