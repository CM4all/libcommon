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
		if (errno == 0 || errno == ENOENT)
			throw FmtRuntimeError("No such user: {:?}", username);
		else
			throw FmtErrno("Failed to look up user {:?}", username);
	}

	uid = pw->pw_uid;
	gid = pw->pw_gid;

	int ngroups = groups.size();
	int n = getgrouplist(username, pw->pw_gid, groups.data(), &ngroups);
	if (n >= 0 && (size_t)n < groups.size())
		groups[n] = 0;
}

void
UidGid::LoadEffective() noexcept
{
	uid = geteuid();
	gid = getegid();
}

char *
UidGid::MakeId(char *p) const noexcept
{
	if (uid != 0)
		p += sprintf(p, ";uid%u", int(uid));

	if (gid != 0)
		p += sprintf(p, ";gid%u", int(gid));

	return p;
}

static bool
IsUid(uid_t uid) noexcept
{
	uid_t ruid, euid, suid;
	return getresuid(&ruid, &euid, &suid) == 0 &&
		uid == ruid && uid == euid && uid == suid;
}

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
	return (uid == 0 || IsUid(uid)) && (gid == 0 || IsGid(gid));
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

	if (gid != 0 && setregid(gid, gid) < 0)
		throw FmtErrno("setgid({}) failed", gid);

	if (HasGroups()) {
		if (setgroups(CountGroups(), groups.data()) < 0)
			throw MakeErrno("setgroups() failed");
	} else if (gid != 0) {
		if (setgroups(0, &gid) < 0)
			throw FmtErrno("setgroups({}) failed", gid);
	}

	if (uid != 0 && setreuid(uid, uid) < 0)
		throw FmtErrno("setuid({}) failed", uid);
}
