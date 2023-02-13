// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Config.hxx"
#include "util/RuntimeError.hxx"

void
SpawnConfig::VerifyUid(uid_t uid) const
{
	if (!IsUidAllowed(uid))
		throw FormatRuntimeError("uid %u is not allowed", unsigned(uid));
}

void
SpawnConfig::VerifyGid(gid_t gid) const
{
	if (!IsGidAllowed(gid))
		throw FormatRuntimeError("gid %u is not allowed", unsigned(gid));
}
