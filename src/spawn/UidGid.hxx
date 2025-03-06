// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <array>
#include <algorithm>

#include <sys/types.h>

struct UidGid {
	/**
	 * Special values for "this uid/gid is not set".
	 */
	static constexpr uid_t UNSET_UID = 0;
	static constexpr gid_t UNSET_GID = 0;

	uid_t real_uid{UNSET_UID};
	gid_t real_gid{UNSET_GID};

	uid_t effective_uid{UNSET_UID};
	gid_t effective_gid{UNSET_GID};

	/**
	 * A list of supplementary groups terminated by #UNSET_GID.
	 */
	std::array<gid_t, 32> supplementary_groups{UNSET_GID};

	/**
	 * Look up a user name in the system user database (/etc/passwd)
	 * and fill #uid, #gid and #supplementary_groups.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Lookup(const char *username);

	void LoadEffective() noexcept;

	constexpr bool IsEmpty() const noexcept {
		return effective_uid == UNSET_UID &&
			effective_gid == UNSET_GID &&
			!HasReal() &&
			!HasSupplementaryGroups();
	}

	constexpr bool IsComplete() const noexcept {
		return effective_uid != UNSET_UID && effective_gid != UNSET_GID;
	}

	constexpr bool HasReal() const noexcept {
		return real_uid != UNSET_UID || real_gid != UNSET_GID;
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

	bool HasSupplementaryGroups() const noexcept {
		return supplementary_groups.front() != UNSET_GID;
	}

	size_t CountSupplementaryGroups() const noexcept {
		return std::distance(supplementary_groups.begin(),
				     std::find(supplementary_groups.begin(), supplementary_groups.end(), UNSET_GID));
	}

	char *MakeId(char *p) const noexcept;

	/**
	 * Throws std::system_error on error.
	 */
	void Apply() const;
};
