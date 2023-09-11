// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

struct SystemdScopeProcess {
	int local_pid, real_pid;
	UniqueFileDescriptor pipe_w;
};

/**
 * Start the "scope" process which does nothing but hold the systemd
 * scope.  It will be moved to a special sub-cgroup called "_" where
 * it idles until the pipe gets closed.  It doesn't do anything else,
 * so throttling it due to memcg constraints will not affect the real
 * spawner process.
 */
SystemdScopeProcess
StartSystemdScopeProcess(bool pid_namespace);
