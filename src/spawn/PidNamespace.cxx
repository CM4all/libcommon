// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PidNamespace.hxx"
#include "spawn/daemon/Client.hxx"
#include "system/Error.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <sched.h>

/**
 * Obtain a PID namespace from the Spawn daemon.
 */
static UniqueFileDescriptor
GetPidNS(const char *name)
{
	return SpawnDaemon::MakePidNamespace(SpawnDaemon::Connect(), name);
}

void
ReassociatePidNamespace(const char *name)
{
	assert(name != nullptr);

	if (setns(GetPidNS(name).Get(), CLONE_NEWPID) < 0)
		throw FormatErrno("Failed to reassociate with PID namespace '%s'",
				  name);
}
