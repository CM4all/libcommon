// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <sys/mount.h>

#include <cstddef> // for std::size_t
#include <cstdint> // for uint_least64_t

#ifndef MOUNT_ATTR_RDONLY
/* fallback definitions for glibc < 2.36 */
#define MOUNT_ATTR_RDONLY 0x00000001
#define MOUNT_ATTR_NOSUID 0x00000002
#define MOUNT_ATTR_NODEV 0x00000004
#define MOUNT_ATTR_NOEXEC 0x00000008
#define FSCONFIG_SET_STRING 1
#define FSCONFIG_CMD_CREATE 6
#define MOVE_MOUNT_F_EMPTY_PATH 0x00000004
#define OPEN_TREE_CLONE 1
#endif

#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif

#ifndef AT_NO_AUTOMOUNT
#define AT_NO_AUTOMOUNT 0x800
#endif

#ifndef AT_RECURSIVE
#define AT_RECURSIVE 0x8000
#endif

class FileDescriptor;
class UniqueFileDescriptor;

UniqueFileDescriptor
FSOpen(const char *fsname);

void
FSConfig(FileDescriptor fs, unsigned cmd,
	 const char *key, const char *value, int aux=0);

UniqueFileDescriptor
FSMount(FileDescriptor fs, unsigned flags);

void
MoveMount(FileDescriptor from_directory, const char *from_path,
	  FileDescriptor to_directory, const char *to_path,
	  unsigned flags);

void
MountSetAttr(FileDescriptor directory, const char *path, unsigned flags,
	     const struct mount_attr *uattr, std::size_t usize);

void
MountSetAttr(FileDescriptor directory, const char *path, unsigned flags,
	     uint_least64_t attr_set, uint_least64_t attr_clr,
	     uint_least64_t propagation=0);

UniqueFileDescriptor
OpenTree(FileDescriptor directory, const char *path, unsigned flags);

void
MountOrThrow(const char *source, const char *target,
	     const char *filesystemtype, unsigned long mountflags,
	     const void *data);

/**
 * Throws std::system_error on error.
 */
void
BindMount(const char *source, const char *target);

void
Umount(const char *target, int flags);
