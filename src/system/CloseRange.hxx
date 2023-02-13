// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_close_range
#define __NR_close_range 436
#endif

#ifndef CLOSE_RANGE_UNSHARE
#define CLOSE_RANGE_UNSHARE (1U << 1)
#endif

static inline int
sys_close_range(unsigned fd, unsigned max_fd, unsigned flags) noexcept
{
	return syscall(__NR_close_range, fd, max_fd, flags);
}
