// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "NetworkNamespace.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/Open.hxx"

#include <sched.h>

/**
 * Open a network namespace in /run/netns.
 */
static UniqueFileDescriptor
OpenNetworkNS(const char *name)
{
	char path[4096];
	if (snprintf(path, sizeof(path),
		     "/run/netns/%s", name) >= (int)sizeof(path))
		throw std::runtime_error("Network namespace name is too long");

	return OpenReadOnly(path);
}

void
ReassociateNetworkNamespace(const char *name)
{
	assert(name != nullptr);

	if (setns(OpenNetworkNS(name).Get(), CLONE_NEWNET) < 0)
		throw FmtErrno("Failed to reassociate with network namespace '{}'",
			       name);
}
