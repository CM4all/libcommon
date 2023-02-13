// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <linux/sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef CLONE_CLEAR_SIGHAND
#define CLONE_CLEAR_SIGHAND 0x100000000ULL
#endif

static inline long
clone3(const struct clone_args *cl_args, size_t size)
{
	return syscall(__NR_clone3, cl_args, size);
}
