// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef CAPABILITY_GLUE_HXX
#define CAPABILITY_GLUE_HXX

/**
 * Does the current process have CAP_SYS_ADMIN?
 */
[[gnu::pure]]
bool
IsSysAdmin() noexcept;

/**
 * Does the current process have CAP_SETUID?
 */
[[gnu::pure]]
bool
HaveSetuid() noexcept;

/**
 * Does the current process have CAP_NET_BIND_SERVICE?
 */
[[gnu::pure]]
bool
HaveNetBindService() noexcept;

#endif
