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

#define __NR_listmount 458

#define LSMT_ROOT 0xffffffffffffffff

struct mnt_id_req {
	__u32 size;
	__u32 spare;
	__u64 mnt_id;
	__u64 param;
};

#endif // Linux < 6.8.0

static inline int
listmount(const struct mnt_id_req *req,
	  __u64 *mnt_ids, size_t nr_mnt_ids,
	  unsigned flags)
{
	return syscall(__NR_listmount, req, mnt_ids, nr_mnt_ids, flags);
}
