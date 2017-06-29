/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef BENG_PROXY_SPAWN_CONFIG_HXX
#define BENG_PROXY_SPAWN_CONFIG_HXX

#include "UidGid.hxx"
#include "util/RuntimeError.hxx"

#include <inline/compiler.h>

#include <set>

/**
 * Configuration for the spawner.
 */
struct SpawnConfig {
    /**
     * If non-empty, then a new systemd scope is created for the
     * spawner process.
     */
    std::string systemd_scope;

    std::string systemd_scope_description;

    UidGid default_uid_gid;

    std::set<uid_t> allowed_uids;
    std::set<gid_t> allowed_gids;

    /**
     * Ignore #allowed_uids and #allowed_gids, and allow all uids/gids
     * (except for root:root)?  This is a kludge for the Workshop
     * project for backwards compatibility with version 1.
     */
    bool allow_any_uid_gid = false;

    void VerifyUid(uid_t uid) const {
        if (allowed_uids.find(uid) == allowed_uids.end())
            throw FormatRuntimeError("uid %d is not allowed", int(uid));
    }

    void VerifyGid(gid_t gid) const {
        if (allowed_gids.find(gid) == allowed_gids.end())
            throw FormatRuntimeError("gid %d is not allowed", int(gid));
    }

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

        VerifyUid(uid_gid.uid);
        VerifyGid(uid_gid.gid);
        VerifyGroups(uid_gid.groups.begin(), uid_gid.groups.end());
    }
};

#endif
