/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SyscallFilter.hxx"
#include "SeccompFilter.hxx"

/**
 * The system calls which are forbidden unconditionally.
 */
static constexpr int forbidden_syscalls[] = {
    SCMP_SYS(adjtimex),
    SCMP_SYS(delete_module),
    SCMP_SYS(init_module),

    /* ptrace() is dangerous because it allows breaking out of
       namespaces */
    SCMP_SYS(ptrace),

    SCMP_SYS(reboot),
    SCMP_SYS(settimeofday),
    SCMP_SYS(swapoff),
    SCMP_SYS(swapon),
};

void
BuildSyscallFilter(Seccomp::Filter &sf)
{
    /* forbid a bunch of dangerous system calls */

    for (auto i : forbidden_syscalls)
        sf.AddRule(SCMP_ACT_KILL, i);
}
