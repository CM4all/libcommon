// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <linux/mount.h>
#include <linux/types.h> // for __u32, __u64
#include <sys/syscall.h>
#include <stddef.h> // for size_t
#include <unistd.h> // for syscall()

#ifndef __NR_listmount
#define __NR_listmount 458
#endif

#ifndef LSMT_ROOT
#define LSMT_ROOT 0xffffffffffffffff
#endif

struct mnt_id_req {
	__u32 size;
	__u32 spare;
	__u64 mnt_id;
	__u64 param;
};

static inline int
listmount(const struct mnt_id_req *req,
	  __u64 *mnt_ids, size_t nr_mnt_ids,
	  unsigned flags)
{
	return syscall(__NR_listmount, req, mnt_ids, nr_mnt_ids, flags);
}
