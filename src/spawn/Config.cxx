// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Config.hxx"
#include "lib/fmt/RuntimeError.hxx"

void
SpawnConfig::VerifyUid(uid_t uid) const
{
	if (!IsUidAllowed(uid))
		throw FmtRuntimeError("uid {} is not allowed", uid);
}

void
SpawnConfig::VerifyGid(gid_t gid) const
{
	if (!IsGidAllowed(gid))
		throw FmtRuntimeError("gid {} is not allowed", gid);
}

void
SpawnConfig::Verify(const UidGid &uid_gid) const
{
	if (uid_gid.real_uid == UidGid::ILLEGAL_UID ||
	    uid_gid.effective_uid == UidGid::ILLEGAL_UID ||
	    uid_gid.real_gid == UidGid::ILLEGAL_GID ||
	    uid_gid.effective_gid == UidGid::ILLEGAL_GID)
		throw std::runtime_error{"uid/gid -1 is illegal"};

	if (allow_any_uid_gid)
		return;

	if (uid_gid.real_uid != UidGid::UNSET_UID)
		VerifyUid(uid_gid.real_uid);

	if (uid_gid.real_gid != UidGid::UNSET_GID)
		VerifyGid(uid_gid.real_gid);

	VerifyUid(uid_gid.effective_uid);
	VerifyGid(uid_gid.effective_gid);
	VerifyGroups(uid_gid.supplementary_groups.begin(),
		     uid_gid.supplementary_groups.end());
}
