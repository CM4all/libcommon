// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PidNamespace.hxx"
#include "accessory/Client.hxx"
#include "lib/fmt/SystemError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <sched.h>

void
ReassociatePidNamespace(std::string_view name)
{
	const auto response = SpawnAccessory::MakeNamespaces(SpawnAccessory::Connect(),
							     name, {.pid = true});

	if (setns(response.pid.Get(), CLONE_NEWPID) < 0)
		throw FmtErrno("Failed to reassociate with PID namespace {:?}",
			       name);
}
