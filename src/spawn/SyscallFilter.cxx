/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SyscallFilter.hxx"
#include "SeccompFilter.hxx"

#include <set>

#include <sys/socket.h>

/**
 * The system calls which are forbidden unconditionally.
 */
static constexpr int forbidden_syscalls[] = {
    SCMP_SYS(acct),
    SCMP_SYS(add_key),
    SCMP_SYS(adjtimex),

#ifdef __NR_bpf
    SCMP_SYS(bpf),
#endif

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

#ifdef __NR_kexec_file_load
    SCMP_SYS(kexec_file_load),
#endif

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

#ifdef __NR_userfaultfd
    SCMP_SYS(userfaultfd),
#endif

    SCMP_SYS(ustat),
    SCMP_SYS(vm86),
    SCMP_SYS(vm86old),
};

/* using a std::set to make sure the list is sorted; now if only there
   was a way to sort a constexpr array at compile time... */
static const std::set<scmp_datum_t> allowed_socket_domains = {
    AF_LOCAL, AF_INET, AF_INET6
};

static void
AddRange(Seccomp::Filter &sf, uint32_t action, int syscall,
         Seccomp::Arg arg, scmp_datum_t begin, scmp_datum_t end)
{
    for (auto i = begin; i != end; ++i)
        sf.AddRule(action, syscall, arg == i);
}

static void
AddInverted(Seccomp::Filter &sf, uint32_t action, int syscall,
            Seccomp::Arg arg, const std::set<scmp_datum_t> &whitelist)
{
    auto i = whitelist.begin();

    sf.AddRule(action, syscall, arg < *i);

    for (auto next = std::next(i), end = whitelist.end();
         next != end; i = next++)
        AddRange(sf, action, syscall, arg, *i + 1, *next);

    sf.AddRule(action, syscall, arg > *i);
}

void
BuildSyscallFilter(Seccomp::Filter &sf)
{
    /* forbid a bunch of dangerous system calls */

    for (auto i : forbidden_syscalls) {
        try {
            sf.AddRule(SCMP_ACT_KILL, i);
        } catch (const std::system_error &e) {
            if (e.code().category() == ErrnoCategory() &&
                e.code().value() == EFAULT) {
                /* system call not supported by this kernel - ignore
                   this problem silently, because an unsupported
                   syscall doesn't need to be filtered */
            } else
                throw;
        }
    }

    /* allow only a few socket domains */

    AddInverted(sf, SCMP_ACT_ERRNO(EAFNOSUPPORT), SCMP_SYS(socket),
                Seccomp::Arg(0), allowed_socket_domains);
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
