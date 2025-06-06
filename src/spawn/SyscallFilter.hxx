// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

namespace Seccomp { class Filter; }

/**
 * Build a standard system call filter.
 *
 * @param sf an existing filter with a SCMP_ACT_ALLOW default actionb
 */
void
BuildSyscallFilter(Seccomp::Filter &sf);

/**
 * Add rules which return EPERM upon attempting to create a new
 * user namespace.
 */
void
ForbidUserNamespace(Seccomp::Filter &sf);

/**
 * Add rules which return EPERM upon attempting to join a multicast
 * group.
 */
void
ForbidMulticast(Seccomp::Filter &sf);

/**
 * Add rules which makes bind() and listen() return EACCES.
 */
void
ForbidBind(Seccomp::Filter &sf);
