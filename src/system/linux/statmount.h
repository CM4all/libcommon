// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <linux/mount.h>
#include <linux/types.h> // for __u32, __u64
#include <linux/version.h>
#include <sys/syscall.h>
#include <stddef.h> // for size_t
#include <unistd.h> // for syscall()

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)

#define __NR_statmount 457

#define STATMOUNT_SB_BASIC 0x00000001U
#define STATMOUNT_MNT_BASIC 0x00000002U
#define STATMOUNT_PROPAGATE_FROM 0x00000004U
#define STATMOUNT_MNT_ROOT 0x00000008U
#define STATMOUNT_MNT_POINT 0x00000010U
#define STATMOUNT_FS_TYPE 0x00000020U

struct mnt_id_req;

struct statmount {
	__u32 size;
	__u32 __spare1;
	__u64 mask;
	__u32 sb_dev_major;
	__u32 sb_dev_minor;
	__u64 sb_magic;
	__u32 sb_flags;
	__u32 fs_type;
	__u64 mnt_id;
	__u64 mnt_parent_id;
	__u32 mnt_id_old;
	__u32 mnt_parent_id_old;
	__u64 mnt_attr;
	__u64 mnt_propagation;
	__u64 mnt_peer_group;
	__u64 mnt_master;
	__u64 propagate_from;
	__u32 mnt_root;
	__u32 mnt_point;
	__u64 __spare2[50];
	//char str[];
};

#endif // Linux < 6.8.0

static inline int
do_statmount(const struct mnt_id_req *req,
	     struct statmount *buf, size_t bufsize,
	     unsigned flags)
{
	return syscall(__NR_statmount, req, buf, bufsize, flags);
}
