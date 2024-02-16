// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <array>
#include <algorithm>

#include <sys/types.h>

struct UidGid {
	uid_t uid;
	gid_t gid;

	std::array<gid_t, 32> groups;

	constexpr UidGid() noexcept:uid(0), gid(0), groups{{0}} {}

	/**
	 * Look up a user name in the system user database (/etc/passwd)
	 * and fill #uid, #gid and #groups.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Lookup(const char *username);

	void LoadEffective() noexcept;

	constexpr bool IsEmpty() const noexcept {
		return uid == 0 && gid == 0 && !HasGroups();
	}

	constexpr bool IsComplete() const noexcept {
		return uid != 0 && gid != 0;
	}

	/**
	 * Is Apply() a no-op?  This can be because no uid/gid is
	 * configured or because the uid/gid is already in effect
	 * (which usually only happens in "debug" mode where the
	 * program runs on a developer machine as regular user and
	 * never switches users).
	 */
	[[gnu::pure]]
	bool IsNop() const noexcept;

	bool HasGroups() const noexcept {
		return groups.front() != 0;
	}

	size_t CountGroups() const noexcept {
		return std::distance(groups.begin(),
				     std::find(groups.begin(), groups.end(), 0));
	}

	char *MakeId(char *p) const noexcept;

	/**
	 * Throws std::system_error on error.
	 */
	void Apply() const;
};
