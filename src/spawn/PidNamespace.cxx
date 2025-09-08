// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PidNamespace.hxx"
#include "accessory/Client.hxx"
#include "lib/fmt/SystemError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <sched.h>

/**
 * Obtain a PID namespace from the Spawn daemon.
 */
static UniqueFileDescriptor
GetPidNS(std::string_view name)
{
	return SpawnAccessory::MakePidNamespace(SpawnAccessory::Connect(), name);
}

void
ReassociatePidNamespace(std::string_view name)
{
	if (setns(GetPidNS(name).Get(), CLONE_NEWPID) < 0)
		throw FmtErrno("Failed to reassociate with PID namespace {:?}",
			       name);
}
