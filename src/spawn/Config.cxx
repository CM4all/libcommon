// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
