/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SyscallFilter.hxx"
#include "SeccompFilter.hxx"

/**
 * The system calls which are forbidden unconditionally.
 */
static constexpr int forbidden_syscalls[] = {
    SCMP_SYS(acct),
    SCMP_SYS(add_key),
    SCMP_SYS(adjtimex),
    SCMP_SYS(bpf),
    SCMP_SYS(clock_adjtime),
    SCMP_SYS(clock_settime),
    SCMP_SYS(create_module),
    SCMP_SYS(delete_module),
    SCMP_SYS(finit_module),
    SCMP_SYS(get_kernel_syms),
    SCMP_SYS(get_mempolicy),
    SCMP_SYS(init_module),
    SCMP_SYS(ioperm),
    SCMP_SYS(iopl),
    SCMP_SYS(kcmp),
    SCMP_SYS(kexec_file_load),
    SCMP_SYS(kexec_load),
    SCMP_SYS(keyctl),
    SCMP_SYS(lookup_dcookie),
    SCMP_SYS(mbind),
    SCMP_SYS(mount),
    SCMP_SYS(move_pages),
    SCMP_SYS(name_to_handle_at),
    SCMP_SYS(perf_event_open),
    SCMP_SYS(personality),
    SCMP_SYS(pivot_root),
    SCMP_SYS(process_vm_readv),
    SCMP_SYS(process_vm_writev),

    /* ptrace() is dangerous because it allows breaking out of
       namespaces */
    SCMP_SYS(ptrace),

    SCMP_SYS(query_module),
    SCMP_SYS(quotactl),
    SCMP_SYS(quotactl),
    SCMP_SYS(reboot),
    SCMP_SYS(request_key),
    SCMP_SYS(set_mempolicy),
    SCMP_SYS(setns),
    SCMP_SYS(settimeofday),
    SCMP_SYS(stime),
    SCMP_SYS(stime),
    SCMP_SYS(swapoff),
    SCMP_SYS(swapon),
    SCMP_SYS(sysfs),
    SCMP_SYS(_sysctl),
    SCMP_SYS(umount),
    SCMP_SYS(umount2),
    SCMP_SYS(unshare),
    SCMP_SYS(uselib),
    SCMP_SYS(userfaultfd),
    SCMP_SYS(ustat),
    SCMP_SYS(vm86),
    SCMP_SYS(vm86old),
};

void
BuildSyscallFilter(Seccomp::Filter &sf)
{
    /* forbid a bunch of dangerous system calls */

    for (auto i : forbidden_syscalls)
        sf.AddRule(SCMP_ACT_KILL, i);
}

static void
ForbidNamespace(Seccomp::Filter &sf, int one_namespace_flag)
{
    using Seccomp::Arg;

    sf.AddRule(SCMP_ACT_ERRNO(EPERM), SCMP_SYS(unshare),
               (Arg(0) & one_namespace_flag) == one_namespace_flag);

    sf.AddRule(SCMP_ACT_ERRNO(EPERM), SCMP_SYS(clone),
               (Arg(0) & one_namespace_flag) == one_namespace_flag);
}

void
ForbidUserNamespace(Seccomp::Filter &sf)
{
    ForbidNamespace(sf, CLONE_NEWUSER);
}
