// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "UserNamespaceOptions.hxx"
#include "MakeId.hxx"
#include "UidGid.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/linux/ProcPid.hxx"
#include "io/linux/UserNamespace.hxx"
#include "AllocatorPtr.hxx"

#include <fmt/core.h>

#include <sched.h> // for CLONE_NEWUSER

using std::string_view_literals::operator""sv;

UserNamespaceOptions::UserNamespaceOptions([[maybe_unused]] AllocatorPtr alloc,
					   const UserNamespaceOptions &src) noexcept
	:fd(src.fd),
	 mapped_real_uid(src.mapped_real_uid),
	 mapped_effective_uid(src.mapped_effective_uid),
	 create(src.create)
{
}

uint_least64_t
UserNamespaceOptions::GetCloneFlags(uint_least64_t flags) const noexcept
{
	if (create)
		flags |= CLONE_NEWUSER;

	return flags;
}

char *
UserNamespaceOptions::FormatUidMap(char *p, const UidGid &uid_gid) const noexcept
{
	const IdMap map{
		.first = {
			.id = uid_gid.effective_uid,
			.mapped_id = mapped_effective_uid > 0 ? mapped_effective_uid : uid_gid.effective_uid,
		},
		.second = {
			.id = uid_gid.real_uid,
			.mapped_id = mapped_real_uid > 0 ? mapped_real_uid : uid_gid.real_uid,
		},
	};

	return FormatIdMap(p, map);
}

char *
UserNamespaceOptions::FormatGidMap(char *p, const UidGid &uid_gid) const noexcept
{
	/* collect all gids (including supplementary groups) in a std::set
	   to eliminate duplicates, and then map them all into the new
	   user namespace */
	std::set<unsigned> gids;

	// TODO: map the current effective gid if no gid was given?
	if (uid_gid.effective_gid != UidGid::UNSET_GID)
		gids.emplace(uid_gid.effective_gid);
	if (uid_gid.real_gid != UidGid::UNSET_GID)
		gids.emplace(uid_gid.real_gid);
	for (unsigned i = 0; uid_gid.supplementary_groups[i] != UidGid::UNSET_GID; ++i)
		gids.emplace(uid_gid.supplementary_groups[i]);

	return FormatIdMap(p, gids);
}

void
UserNamespaceOptions::SetupUidGidMap(const UidGid &uid_gid, unsigned _pid) const
{
	/* collect all gids (including supplementary groups) in a std::set
	   to eliminate duplicates, and then map them all into the new
	   user namespace */
	std::set<unsigned> gids;

	// TODO: map the current effective gid if no gid was given?
	if (uid_gid.effective_gid != UidGid::UNSET_GID)
		gids.emplace(uid_gid.effective_gid);
	if (uid_gid.real_gid != UidGid::UNSET_GID)
		gids.emplace(uid_gid.real_gid);
	for (unsigned i = 0; uid_gid.supplementary_groups[i] != UidGid::UNSET_GID; ++i)
		gids.emplace(uid_gid.supplementary_groups[i]);

	const auto proc_pid = OpenProcPid(_pid);

	if (!gids.empty())
		SetupGidMap(proc_pid, gids);

	const IdMap map{
		.first = {
			.id = uid_gid.effective_uid,
			.mapped_id = mapped_effective_uid > 0 ? mapped_effective_uid : uid_gid.effective_uid,
		},
		.second = {
			.id = uid_gid.real_uid,
			.mapped_id = mapped_real_uid > 0 ? mapped_real_uid : uid_gid.real_uid,
		},
	};

	SetupUidMap(proc_pid, map);
}

char *
UserNamespaceOptions::MakeId(char *p) const noexcept
{
	p = AppendOptional(p, ";uns"sv, create);

	if (mapped_real_uid > 0)
		p = fmt::format_to(p, ";mru{}", mapped_real_uid);

	if (mapped_effective_uid > 0)
		p = fmt::format_to(p, ";meu{}", mapped_effective_uid);

	return p;
}
