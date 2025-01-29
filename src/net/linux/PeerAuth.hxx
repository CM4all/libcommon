// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/PeerCredentials.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <cassert>
#include <string>

class SocketDescriptor;

/**
 * Helper for SO_PEERCRED and SO_PEERPIDFD.
 */
class SocketPeerAuth {
	mutable std::string cgroup_path;

	SocketPeerCredentials cred;

	UniqueFileDescriptor pidfd;

public:
	explicit SocketPeerAuth(SocketDescriptor s) noexcept;

	bool HaveCred() const noexcept {
		return cred.IsDefined();
	}

	pid_t GetPid() const noexcept {
		assert(HaveCred());

		return cred.GetPid();
	}

	uid_t GetUid() const noexcept {
		assert(HaveCred());

		return cred.GetUid();
	}

	gid_t GetGid() const noexcept{
		assert(HaveCred());

		return cred.GetGid();
	}

	std::string_view GetCgroupPath() const;
};
