/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SPAWN_SYSCALL_FILTER_HXX
#define SPAWN_SYSCALL_FILTER_HXX

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

#endif
