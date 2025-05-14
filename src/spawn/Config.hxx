// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "UidGid.hxx"
#include "spawn/config.h"

#ifdef HAVE_LIBSYSTEMD
#include "Systemd.hxx"
#endif

#include <set>
#include <string>

/**
 * Configuration for the spawner.
 */
struct SpawnConfig {
#ifdef HAVE_LIBSYSTEMD
	/**
	 * If non-empty, then a new systemd scope is created for the
	 * spawner process.
	 */
	std::string systemd_scope;

	std::string systemd_scope_description;

	SystemdUnitProperties systemd_scope_properties;

	/**
	 * If non-empty, then the new systemd scope is created in the
	 * specified slice.
	 */
	std::string systemd_slice;
#endif

	/**
	 * Run the spawner itself as this user.
	 */
	UidGid spawner_uid_gid;

	UidGid default_uid_gid;

	std::set<uid_t> allowed_uids;
	std::set<gid_t> allowed_gids;

	/**
	 * If non-zero, then all user ids from this value on are
	 * allowed.
	 */
	uid_t allow_all_uids_from = 0;

	/**
	 * If non-zero, then all cgroups can be managed by this gid.
	 * All cgroups are then owned by this gid and are
	 * group-writable.
	 */
	gid_t cgroups_writable_by_gid = 0;

	/**
	 * Is a systemd scope optional?  This option is only for
	 * debugging/development if launched by an unprivileged user.
	 */
	bool systemd_scope_optional = false;

	/**
	 * Ignore #allowed_uids and #allowed_gids, and allow all uids/gids
	 * (except for root:root)?  This is a kludge for the Workshop
	 * project for backwards compatibility with version 1.
	 */
	bool allow_any_uid_gid = false;

	[[gnu::pure]]
	bool IsUidAllowed(uid_t uid) const noexcept {
		return (allow_all_uids_from > 0 &&
			uid >= allow_all_uids_from) ||
			allowed_uids.find(uid) != allowed_uids.end();
	}

	[[gnu::pure]]
	bool IsGidAllowed(gid_t gid) const noexcept {
		return allowed_gids.find(gid) != allowed_gids.end();
	}

	void VerifyUid(uid_t uid) const;
	void VerifyGid(gid_t gid) const;

	template<typename I>
	void VerifyGroups(I begin, I end) const {
		for (I i = begin; i != end; ++i) {
			if (*i == 0)
				return;

			VerifyGid(*i);
		}
	}

	void Verify(const UidGid &uid_gid) const {
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
};
