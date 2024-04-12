// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline int
my_pidfd_open(pid_t pid, unsigned int flags) noexcept
{
	return syscall(__NR_pidfd_open, pid, flags);
}

static inline int
my_pidfd_send_signal(int pidfd, int sig, siginfo_t *info,
		     unsigned int flags) noexcept
{
	return syscall(__NR_pidfd_send_signal, pidfd, sig, info, flags);
}
