// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <sys/mount.h>

#include <cstddef> // for std::size_t
#include <cstdint> // for uint_least64_t

struct FileAt;
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
MoveMount(FileAt from, FileAt to, unsigned flags);

void
MountSetAttr(FileAt file, unsigned flags,
	     const struct mount_attr *uattr, std::size_t usize);

void
MountSetAttr(FileAt file, unsigned flags,
	     uint_least64_t attr_set, uint_least64_t attr_clr,
	     uint_least64_t propagation=0);

UniqueFileDescriptor
OpenTree(FileAt file, unsigned flags);

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

/**
 * Unmount a filesystem specified by a directory file descriptor and
 * path relative to it.
 *
 * No such system call exists in the Linux kernel, but this
 * implementation does this by using move_mount() to a well-known
 * temporary directory and then unmounting that directory.
 *
 * Throws on error.
 *
 * @param flags move_mount() flags such as MOVE_MOUNT_F_EMPTY_PATH
 */
void
UmountDetachAt(FileAt file,
	       unsigned flags,
	       const char *tmp);
