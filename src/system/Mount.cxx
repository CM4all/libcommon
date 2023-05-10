// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Mount.hxx"
#include "Error.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <errno.h>

#ifndef FSOPEN_CLOEXEC
/* fallback definitions for glibc < 2.36 */

#include <sys/syscall.h>

#define FSOPEN_CLOEXEC 0x00000001

static int
fsopen(const char *__fs_name, unsigned int __flags) noexcept
{
	return syscall(__NR_fsopen, __fs_name, __flags);
}

#define FSMOUNT_CLOEXEC 0x00000001

static int
fsmount(int __fd, unsigned int __flags, unsigned int __ms_flags) noexcept
{
	return syscall(__NR_fsmount, __fd, __flags, __ms_flags);
}

static int
fsconfig(int __fd, unsigned int __cmd, const char *__key,
	 const void *__value, int __aux) noexcept
{
	return syscall(__NR_fsconfig, __fd, __cmd,
		       __key, __value, __aux);
}

static int
move_mount(int __from_dfd, const char *__from_pathname,
	   int __to_dfd, const char *__to_pathname,
	   unsigned int flags) noexcept
{
	return syscall(__NR_move_mount,
		       __from_dfd, __from_pathname,
		       __to_dfd, __to_pathname,
		       flags);
}

#endif // FSOPEN_CLOEXEC

#ifndef MOUNT_ATTR_SIZE_VER0
/* fallback definitions for glibc < 2.36 */

struct mount_attr
{
  uint64_t attr_set;
  uint64_t attr_clr;
  uint64_t propagation;
  uint64_t userns_fd;
};

#define MOUNT_ATTR_SIZE_VER0 32

#ifndef __NR_mount_setattr
#define __NR_mount_setattr 442
#endif

static int
mount_setattr(int __dfd, const char *__path, unsigned int __flags,
	      struct mount_attr *__uattr, std::size_t __usize) noexcept
{
	return syscall(__NR_mount_setattr,
		       __dfd, __path, __flags,
		       __uattr, __usize);
}

#endif // MOUNT_ATTR_SIZE_VER0

#ifndef OPEN_TREE_CLOEXEC
/* fallback definitions for glibc < 2.36 */

#include <fcntl.h> // for O_CLOEXEC
#define OPEN_TREE_CLOEXEC O_CLOEXEC

static int
open_tree(int __dfd, const char *__filename, unsigned int __flags) noexcept
{
	return syscall(__NR_open_tree, __dfd, __filename, __flags);
}

#endif

UniqueFileDescriptor
OpenTree(FileDescriptor directory, const char *path, unsigned flags)
{
	int fd = open_tree(directory.Get(), path, flags|OPEN_TREE_CLOEXEC);
	if (fd < 0)
		throw MakeErrno("open_tree() failed");

	return UniqueFileDescriptor{fd};
}

UniqueFileDescriptor
FSOpen(const char *fsname)
{
	int fs = fsopen(fsname, FSOPEN_CLOEXEC);
	if (fs < 0)
		throw MakeErrno("fsopen() failed");

	return UniqueFileDescriptor{fs};
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

	return UniqueFileDescriptor{mount};
}

void
MoveMount(FileDescriptor from_directory, const char *from_path,
	  FileDescriptor to_directory, const char *to_path,
	  unsigned flags)
{
	if (move_mount(from_directory.Get(), from_path,
		       to_directory.Get(), to_path,
		       flags) < 0)
		throw MakeErrno("move_mount() failed");
}

void
MountSetAttr(FileDescriptor directory, const char *path, unsigned flags,
	     const struct mount_attr *uattr, std::size_t usize)
{
	if (mount_setattr(directory.Get(), path, flags,
			  const_cast<struct mount_attr *>(uattr),
			  usize) < 0)
		throw MakeErrno("mount_setattr() failed");
}

void
MountSetAttr(FileDescriptor directory, const char *path, unsigned flags,
	     uint_least64_t attr_set, uint_least64_t attr_clr,
	     uint_least64_t propagation)
{
	const struct mount_attr attr{
		.attr_set = attr_set,
		.attr_clr = attr_clr,
		.propagation = propagation,
	};

	static_assert(sizeof(attr) >= MOUNT_ATTR_SIZE_VER0);

	MountSetAttr(directory, path, flags,
		     &attr, MOUNT_ATTR_SIZE_VER0);
}

void
MountOrThrow(const char *source, const char *target,
	     const char *filesystemtype, unsigned long mountflags,
	     const void *data)
{
	if (mount(source, target, filesystemtype, mountflags, data) < 0)
		throw FormatErrno("mount('%s') failed", target);
}

void
BindMount(const char *source, const char *target)
{
	if (mount(source, target, nullptr, MS_BIND, nullptr) < 0)
		throw FormatErrno("bind_mount('%s', '%s') failed", source, target);
}

void
Umount(const char *target, int flags)
{
	if (umount2(target, flags) < 0)
		throw FormatErrno("umount('%s') failed", target);
}

void
UmountDetachAt(FileDescriptor fd, const char *path,
	       unsigned flags,
	       const char *tmp)
{
	MoveMount(fd, path,
		  FileDescriptor{AT_FDCWD}, tmp,
		  flags);
	Umount(tmp, MNT_DETACH);
}
