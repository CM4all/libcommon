// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SyscallFilter.hxx"
#include "SeccompFilter.hxx"

#include <set>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sched.h>

#ifndef __NR_clone3
/* this is needed on Debian Buster and older */
#define __NR_clone3 435
#endif

#ifndef __SNR_listmount
#ifndef __NR_listmount
#include "system/linux/listmount.h"
#endif
#define __SNR_listmount __NR_listmount
#endif

#ifndef __SNR_statmount
#ifndef __NR_statmount
#include "system/linux/statmount.h"
#endif
#define __SNR_statmount __NR_statmount
#endif

/**
 * The system calls which are disabled unconditionally and fail by
 * returning ENOSYS.
 */
static constexpr int disable_syscalls[] = {
	SCMP_SYS(get_mempolicy),
};

/**
 * Like #disable_syscalls, but kill the process with SIGSYS instead of
 * returning ENOSYS.
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
	SCMP_SYS(fanotify_init),
	SCMP_SYS(fanotify_mark),
	SCMP_SYS(finit_module),
	SCMP_SYS(get_kernel_syms),
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
	SCMP_SYS(move_pages),
	SCMP_SYS(name_to_handle_at),
	SCMP_SYS(nfsservctl),
	SCMP_SYS(perf_event_open),
	SCMP_SYS(personality),
	SCMP_SYS(process_vm_readv),
	SCMP_SYS(process_vm_writev),

	/* ptrace() is dangerous because it allows breaking out of
	   namespaces */
	SCMP_SYS(ptrace),

	SCMP_SYS(query_module),
	SCMP_SYS(reboot),
	SCMP_SYS(request_key),
	SCMP_SYS(set_mempolicy),
	SCMP_SYS(setns),
	SCMP_SYS(settimeofday),
	SCMP_SYS(stime),
	SCMP_SYS(swapoff),
	SCMP_SYS(swapon),
	SCMP_SYS(sysfs),
	SCMP_SYS(syslog),
	SCMP_SYS(_sysctl),
	SCMP_SYS(uselib),

#ifdef __NR_userfaultfd
	SCMP_SYS(userfaultfd),
#endif

	SCMP_SYS(ustat),
	SCMP_SYS(vm86),
	SCMP_SYS(vm86old),

	SCMP_SYS(listmount),
	SCMP_SYS(statmount),

	/* we used to forbid quotactl(), but on one hand, we need it
	   for certain internal services, and on the other hand,
	   allowing it doesn't cause any harm; users can check their
	   own quotas (which is OK), but they can't modify them */
	//SCMP_SYS(quotactl),
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

	for (auto i : disable_syscalls) {
		try {
			sf.AddRule(SCMP_ACT_ERRNO(ENOSYS), i);
		} catch (const std::system_error &e) {
			if (IsErrno(e, EFAULT)) {
				/* system call not supported by this kernel - ignore
				   this problem silently, because an unsupported
				   syscall doesn't need to be filtered */
			} else
				throw;
		}
	}

	for (auto i : forbidden_syscalls) {
		try {
			sf.AddRule(SCMP_ACT_KILL, i);
		} catch (const std::system_error &e) {
			if (IsErrno(e, EFAULT)) {
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

	/* we can't inspect the clone3() flags parameter because we
	   can't dereference "struct clone_args" - so let's pretend
	   this kernel doesn't support clone3() */
	try {
		sf.AddRule(SCMP_ACT_ERRNO(ENOSYS), SCMP_SYS(clone3));
	} catch (const std::system_error &e) {
		if (IsErrno(e, EFAULT)) {
			/* system call not supported by this kernel -
			   ignore this problem silently, because an
			   unsupported syscall doesn't need to be
			   filtered */
		} else
			throw;
	}
}

void
ForbidUserNamespace(Seccomp::Filter &sf)
{
	ForbidNamespace(sf, CLONE_NEWUSER);
}

template<typename C>
static void
AddSetSockOpts(Seccomp::Filter &sf, uint32_t action,
	       int level, const C &optnames)
{
	using Seccomp::Arg;

	for (int optname : optnames)
		sf.AddRule(action, SCMP_SYS(setsockopt),
			   Arg(1) == level,
			   Arg(2) == optname);
}

void
ForbidMulticast(Seccomp::Filter &sf)
{
	static constexpr int forbidden_ip[] = {
		IP_ADD_MEMBERSHIP,
		IP_ADD_SOURCE_MEMBERSHIP,
		IP_BLOCK_SOURCE,
		IP_DROP_MEMBERSHIP,
		IP_DROP_SOURCE_MEMBERSHIP,
		IP_MULTICAST_ALL,
		IP_MULTICAST_IF,
		IP_MULTICAST_LOOP,
		IP_MULTICAST_TTL,
		IP_UNBLOCK_SOURCE,
	};

	AddSetSockOpts(sf, SCMP_ACT_ERRNO(EPERM), IPPROTO_IP, forbidden_ip);

	static constexpr int forbidden_ipv6[] = {
		IPV6_ADD_MEMBERSHIP,
		IPV6_DROP_MEMBERSHIP,
		IPV6_MULTICAST_HOPS,
		IPV6_MULTICAST_IF,
		IPV6_MULTICAST_LOOP,
	};

	AddSetSockOpts(sf, SCMP_ACT_ERRNO(EPERM), IPPROTO_IPV6, forbidden_ipv6);
}

void
ForbidBind(Seccomp::Filter &sf)
{
	sf.AddRule(SCMP_ACT_ERRNO(EACCES), SCMP_SYS(bind));
	sf.AddRule(SCMP_ACT_ERRNO(EACCES), SCMP_SYS(listen));
}
