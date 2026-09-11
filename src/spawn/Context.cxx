// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Context.hxx"
#include "spawn/config.h" // for HAVE_LIBCAP
#include "system/linux/Mount.hxx"

#include <errno.h>
#include <unistd.h> // for close(), geteuid()

#ifdef HAVE_LIBCAP
#include "lib/cap/Glue.hxx" // for IsSysAdmin()
#else

[[gnu::pure]]
static bool
IsSysAdmin() noexcept
{
	return geteuid() == 0;
}

#endif // !HAVE_LIBCAP

[[gnu::const]]
static bool
HaveOpenTreeNamespace() noexcept
{
	int fd = open_tree(-1, "/", OPEN_TREE_NAMESPACE|OPEN_TREE_CLOEXEC);
	if (fd >= 0) {
		close(fd);
		return true;
	}

	return errno != EINVAL;
}

SpawnContext::SpawnContext() noexcept
	:is_sys_admin(IsSysAdmin()),
	 have_open_tree_namespace(HaveOpenTreeNamespace()) {}
