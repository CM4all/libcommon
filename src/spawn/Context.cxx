// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Context.hxx"
#include "spawn/config.h" // for HAVE_LIBCAP

#ifdef HAVE_LIBCAP
#include "lib/cap/Glue.hxx" // for IsSysAdmin()
#else
#include <unistd.h> // for geteuid()

[[gnu::pure]]
static bool
IsSysAdmin() noexcept
{
	return geteuid() == 0;
}

#endif // !HAVE_LIBCAP

SpawnContext::SpawnContext() noexcept
	:is_sys_admin(IsSysAdmin()) {}
