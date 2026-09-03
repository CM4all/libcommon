// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Mount.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/FileAt.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fcntl.h> // for AT_FDCWD

UniqueFileDescriptor
OpenTree(FileAt file, unsigned flags)
{
	int fd = open_tree(file.directory.Get(), file.name, flags|OPEN_TREE_CLOEXEC);
	if (fd < 0)
		throw MakeErrno("open_tree() failed");

	return UniqueFileDescriptor{AdoptTag{}, fd};
}

UniqueFileDescriptor
FSOpen(const char *fsname)
{
	int fs = fsopen(fsname, FSOPEN_CLOEXEC);
	if (fs < 0)
		throw MakeErrno("fsopen() failed");

	return UniqueFileDescriptor{AdoptTag{}, fs};
}

void
FSConfig(FileDescriptor fs, unsigned cmd,
	 const char *key, const char *value, int aux)
{
	if (fsconfig(fs.Get(), cmd, key, value, aux) < 0)
		throw MakeErrno("fsconfig() failed");
}

UniqueFileDescriptor
FSMount(FileDescriptor fs, unsigned flags)
{
	int mount = fsmount(fs.Get(), FSMOUNT_CLOEXEC, flags);
	if (mount < 0)
		throw MakeErrno("fsmount() failed");

	return UniqueFileDescriptor{AdoptTag{}, mount};
}

void
MoveMount(FileAt from, FileAt to, unsigned flags)
{
	if (move_mount(from.directory.Get(), from.name,
		       to.directory.Get(), to.name,
		       flags) < 0)
		throw MakeErrno("move_mount() failed");
}

void
MountSetAttr(FileAt file, unsigned flags,
	     const struct mount_attr *uattr, std::size_t usize)
{
	if (mount_setattr(file.directory.Get(), file.name, flags,
			  const_cast<struct mount_attr *>(uattr),
			  usize) < 0)
		throw MakeErrno("mount_setattr() failed");
}

void
MountSetAttr(FileAt file, unsigned flags,
	     uint_least64_t attr_set, uint_least64_t attr_clr,
	     uint_least64_t propagation)
{
	const struct mount_attr attr{
		.attr_set = attr_set,
		.attr_clr = attr_clr,
		.propagation = propagation,
	};

	static_assert(sizeof(attr) >= MOUNT_ATTR_SIZE_VER0);

	MountSetAttr(file, flags,
		     &attr, MOUNT_ATTR_SIZE_VER0);
}

void
MountOrThrow(const char *source, const char *target,
	     const char *filesystemtype, unsigned long mountflags,
	     const void *data)
{
	if (mount(source, target, filesystemtype, mountflags, data) < 0)
		throw FmtErrno("mount({:?}) failed", target);
}

void
BindMount(const char *source, const char *target)
{
	if (mount(source, target, nullptr, MS_BIND, nullptr) < 0)
		throw FmtErrno("bind_mount({:?}, {:?}) failed", source, target);
}

void
Umount(const char *target, int flags)
{
	if (umount2(target, flags) < 0)
		throw FmtErrno("umount({:?}) failed", target);
}

void
UmountDetachAt(FileAt file,
	       unsigned flags,
	       const char *tmp)
{
	MoveMount(file,
		  {FileDescriptor{AT_FDCWD}, tmp},
		  flags);
	Umount(tmp, MNT_DETACH);
}
