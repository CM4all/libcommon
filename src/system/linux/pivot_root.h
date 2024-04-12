// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Provide access to the pivot_root() system call.
 */

#ifndef PIVOT_ROOT_H
#define PIVOT_ROOT_H

#include <sys/syscall.h>
#include <unistd.h>

static inline int
my_pivot_root(const char *new_root, const char *put_old)
{
    return syscall(__NR_pivot_root, new_root, put_old);
}

#endif
