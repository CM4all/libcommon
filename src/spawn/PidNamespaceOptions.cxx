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
	 mode(src.mode)
{
}

uint_least64_t
PidNamespaceOptions::GetCloneFlags(uint_least64_t flags) const noexcept
{
	switch (mode) {
	case Mode::DISABLED:
		break;

	case Mode::ANONYMOUS:
		/* create an anonymous PID namespace with clone() */
		flags |= CLONE_NEWPID;
		break;

	case Mode::ACCESSORY:
		/* don't create a PID namespace - an existing PID
		   namespace is being reassociated with setns() */
		break;
	}

	return flags;
}

char *
PidNamespaceOptions::MakeId(char *p) const noexcept
{
	switch (mode) {
	case Mode::DISABLED:
		break;

	case Mode::ANONYMOUS:
		p = (char *)mempcpy(p, ";pns", 4);
		break;

	case Mode::ACCESSORY:
		p = (char *)mempcpy(p, ";pns=", 5);
		p = (char *)stpcpy(p, name);
		break;
	}

	return p;
}
