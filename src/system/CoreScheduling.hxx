// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <sys/prctl.h>

#ifndef PR_SCHED_CORE
#define PR_SCHED_CORE 62
#endif

#ifndef PR_SCHED_CORE_CREATE
#define PR_SCHED_CORE_CREATE 1
#endif

/**
 * @see
 * https://www.kernel.org/doc/html/latest/admin-guide/hw-vuln/core-scheduling.html
 */
namespace CoreScheduling {

inline bool
Create(pid_t pid) noexcept
{
	return prctl(PR_SCHED_CORE, PR_SCHED_CORE_CREATE,
		     pid, 0 /*PIDTYPE_PID*/,
		     nullptr) == 0;
}

}
