/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SyscallFilter.hxx"
#include "SeccompFilter.hxx"

void
BuildSyscallFilter(Seccomp::Filter &sf)
{
    /* forbid a bunch of dangerous system calls */

    sf.AddRule(SCMP_ACT_KILL, SCMP_SYS(init_module));
    sf.AddRule(SCMP_ACT_KILL, SCMP_SYS(delete_module));
    sf.AddRule(SCMP_ACT_KILL, SCMP_SYS(reboot));
    sf.AddRule(SCMP_ACT_KILL, SCMP_SYS(settimeofday));
    sf.AddRule(SCMP_ACT_KILL, SCMP_SYS(adjtimex));
    sf.AddRule(SCMP_ACT_KILL, SCMP_SYS(swapon));
    sf.AddRule(SCMP_ACT_KILL, SCMP_SYS(swapoff));

    /* ptrace() is dangerous because it allows breaking out of
       namespaces */
    sf.AddRule(SCMP_ACT_KILL, SCMP_SYS(ptrace));
}
