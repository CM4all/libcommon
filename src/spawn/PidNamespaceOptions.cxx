// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PidNamespaceOptions.hxx"
#include "NetworkNamespace.hxx"
#include "UidGid.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "io/linux/UserNamespace.hxx"

#include <fmt/core.h>

#include <set>

#include <assert.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>

PidNamespaceOptions::PidNamespaceOptions(AllocatorPtr alloc,
					 const PidNamespaceOptions &src) noexcept
	:name(alloc.CheckDup(src.name)),
	 enable(src.enable)
{
}

uint_least64_t
PidNamespaceOptions::GetCloneFlags(uint_least64_t flags) const noexcept
{
	if (enable && name == nullptr)
		flags |= CLONE_NEWPID;

	return flags;
}

char *
PidNamespaceOptions::MakeId(char *p) const noexcept
{
	if (enable)
		p = (char *)mempcpy(p, ";pns", 4);

	if (name != nullptr) {
		p = (char *)mempcpy(p, ";pns=", 5);
		p = (char *)stpcpy(p, name);
	}

	return p;
}
