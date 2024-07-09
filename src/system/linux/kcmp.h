// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <linux/kcmp.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline int
kcmp(pid_t pid1, pid_t pid2, int type, unsigned long idx1, unsigned long idx2) noexcept
{
    return syscall(__NR_kcmp, pid1, pid2, type, idx1, idx2);
}
