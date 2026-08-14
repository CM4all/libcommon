// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "NamespaceOptions.hxx"
#include "MakeId.hxx"
#include "NetworkNamespace.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/linux/ProcPid.hxx"
#include "io/linux/UserNamespace.hxx"

#include <set>

#include <assert.h>
#include <sched.h>
#include <unistd.h>

using std::string_view_literals::operator""sv;

NamespaceOptions::NamespaceOptions(AllocatorPtr alloc,
				   const NamespaceOptions &src) noexcept
	:enable_cgroup(src.enable_cgroup),
	 enable_network(src.enable_network),
	 enable_ipc(src.enable_ipc),
	 network_namespace_name(alloc.CheckDup(src.network_namespace_name)),
	 hostname(alloc.CheckDup(src.hostname)),
	 user(alloc, src.user),
	 pid(alloc, src.pid),
	 mount(alloc, src.mount),
	 ipc_namespace(src.ipc_namespace)
{
}

#if TRANSLATION_ENABLE_EXPAND

bool
NamespaceOptions::IsExpandable() const noexcept
{
	return mount.IsExpandable();
}

void
NamespaceOptions::Expand(AllocatorPtr alloc, const MatchData &match_data)
{
	mount.Expand(alloc, match_data);
}

#endif

uint_least64_t
NamespaceOptions::GetCloneFlags(uint_least64_t flags) const noexcept
{
	flags = user.GetCloneFlags(flags);
	flags = pid.GetCloneFlags(flags);
	if (enable_cgroup)
		flags |= CLONE_NEWCGROUP;
	if (enable_network)
		flags |= CLONE_NEWNET;
	if (enable_ipc)
		flags |= CLONE_NEWIPC;
	if (mount.IsEnabled())
		flags |= CLONE_NEWNS;
	if (hostname != nullptr)
		flags |= CLONE_NEWUTS;

	return flags;
}

void
NamespaceOptions::ReassociateNetwork() const
{
	assert(network_namespace_name != nullptr);

	ReassociateNetworkNamespace(network_namespace_name);
}

void
NamespaceOptions::Apply(const UidGid &uid_gid) const
{
	if (user.IsEnabled())
		// TODO eliminate this OpenProcPid() call
		DenySetGroups(OpenProcPid(0));

	/* set up UID/GID mapping in the old /proc */
	if (user.create)
		user.SetupUidGidMap(uid_gid, 0);

	if (network_namespace_name != nullptr)
		ReassociateNetwork();

	if (ipc_namespace.IsDefined() &&
	    setns(ipc_namespace.Get(), CLONE_NEWIPC) < 0)
		throw MakeErrno("Failed to reassociate with IPC namespace");

	mount.Apply(uid_gid);

	if (hostname != nullptr &&
	    sethostname(hostname, strlen(hostname)) < 0)
		throw MakeErrno("sethostname() failed");

	/* reassociate with the selected user namespace at the end
	   after all privileged operations are done, because that will
	   drop all capabilities */
	if (user.fd.IsDefined() &&
	    setns(user.fd.Get(), CLONE_NEWUSER) < 0)
		throw MakeErrno("Failed to reassociate with user namespace");
}

void
NamespaceOptions::ApplyNetwork() const
{
	if (network_namespace_name != nullptr)
		ReassociateNetwork();
	else if (enable_network) {
		if (unshare(CLONE_NEWNET) < 0)
			throw MakeErrno("unshare(CLONE_NEWNET) failed");
	}
}

char *
NamespaceOptions::MakeId(char *p) const noexcept
{
	p = user.MakeId(p);
	p = AppendOptional(p, ";cns"sv, enable_cgroup);

	if (enable_network) {
		p = AppendString(p, ";nns"sv);
		p = AppendOptionalValue(p, "="sv, network_namespace_name);
	}

	p = AppendOptional(p, ";ins"sv, enable_ipc);

	p = pid.MakeId(p);
	p = mount.MakeId(p);

	p = AppendOptionalValue(p, ";uts="sv, hostname);

	return p;
}
