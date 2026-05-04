// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PidNamespaceOptions.hxx"
#include "MakeId.hxx"
#include "AllocatorPtr.hxx"

#include <sched.h> // for CLONE_NEWPID

using std::string_view_literals::operator""sv;

PidNamespaceOptions::PidNamespaceOptions(AllocatorPtr alloc,
					 const PidNamespaceOptions &src) noexcept
	:name(alloc.CheckDup(src.name)),
	 fd(src.fd),
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
		p = AppendString(p, ";pns"sv);
		break;

	case Mode::ACCESSORY:
		p = AppendValue(p, ";pns="sv, name);
		break;
	}

	return p;
}
