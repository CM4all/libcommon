// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "UidGid.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"

#include <unistd.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>

void
UidGid::Lookup(const char *username)
{
	errno = 0;
	const auto *pw = getpwnam(username);
	if (pw == nullptr) {
		if (const int e = errno; e == 0 || e == ENOENT)
			throw FmtRuntimeError("No such user: {:?}", username);
		else
			throw FmtErrno(e, "Failed to look up user {:?}", username);
	}

	effective_uid = pw->pw_uid;
	effective_gid = pw->pw_gid;

	int ngroups = supplementary_groups.size();
	int n = getgrouplist(username, pw->pw_gid, supplementary_groups.data(), &ngroups);
	if (n >= 0 && (size_t)n < supplementary_groups.size())
		supplementary_groups[n] = UNSET_GID;
}

void
UidGid::LoadEffective() noexcept
{
	effective_uid = geteuid();
	effective_gid = getegid();
}

char *
UidGid::MakeId(char *p) const noexcept
{
	if (effective_uid != UNSET_UID)
		p = fmt::format_to(p, ";uid{}", effective_uid);

	if (effective_gid != UNSET_GID)
		p = fmt::format_to(p, ";gid{}", effective_gid);

	return p;
}

[[gnu::pure]]
static bool
IsUid(uid_t uid) noexcept
{
	uid_t ruid, euid, suid;
	return getresuid(&ruid, &euid, &suid) == 0 &&
		uid == ruid && uid == euid && uid == suid;
}

[[gnu::pure]]
static bool
IsGid(gid_t gid) noexcept
{
	gid_t rgid, egid, sgid;
	return getresgid(&rgid, &egid, &sgid) == 0 &&
		gid == rgid && gid == egid && gid == sgid;
}

bool
UidGid::IsNop() const noexcept
{
	return (effective_uid == UNSET_UID || IsUid(effective_uid)) &&
		(effective_gid == UNSET_GID || IsGid(effective_gid));
}

void
UidGid::Apply() const
{
	if (IsNop())
		/* skip if we're already the configured (unprivileged)
		   uid/gid; also don't try setgroups(), because that will fail
		   anyway if we're unprivileged; unprivileged operation is
		   only for debugging anyway, so that's ok */
		return;

	if (effective_gid != UNSET_GID) {
		const gid_t new_real_gid = real_gid != UNSET_GID ? real_gid : effective_gid;

		if (setregid(new_real_gid, effective_gid) < 0)
			throw FmtErrno("setregid({}, {}) failed", new_real_gid, effective_gid);
	}

	if (const auto n_groups = CountSupplementaryGroups(); n_groups > 0) {
		if (setgroups(n_groups, supplementary_groups.data()) < 0)
			throw MakeErrno("setgroups() failed");
	} else if (effective_gid != UNSET_GID) {
		if (setgroups(0, &effective_gid) < 0)
			throw FmtErrno("setgroups({}) failed", effective_gid);
	}

	if (effective_uid != UNSET_UID) {
		const uid_t new_real_uid = real_uid != UNSET_UID ? real_uid : effective_uid;

		if (setreuid(new_real_uid, effective_uid) < 0)
			throw FmtErrno("setreuid({}, {}) failed", new_real_uid, effective_uid);
	}
}
