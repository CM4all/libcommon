// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef IOPRIO_HXX
#define IOPRIO_HXX

#include <sys/syscall.h>
#include <unistd.h>

static int
ioprio_set(int which, int who, int ioprio) noexcept
{
	return syscall(__NR_ioprio_set, which, who, ioprio);
}

static void
ioprio_set_idle() noexcept
{
	static constexpr int _IOPRIO_WHO_PROCESS = 1;
	static constexpr int _IOPRIO_CLASS_IDLE = 3;
	static constexpr int _IOPRIO_CLASS_SHIFT = 13;
	static constexpr int _IOPRIO_IDLE =
		(_IOPRIO_CLASS_IDLE << _IOPRIO_CLASS_SHIFT) | 7;

	ioprio_set(_IOPRIO_WHO_PROCESS, 0, _IOPRIO_IDLE);
}

#endif
