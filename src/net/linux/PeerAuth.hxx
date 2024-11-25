// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

#include <cassert>
#include <string>

#include <sys/socket.h> // for struct ucred

class SocketDescriptor;

/**
 * Helper for SO_PEERCRED and SO_PEERPIDFD.
 */
class SocketPeerAuth {
	mutable std::string cgroup_path;

	const struct ucred cred;

	UniqueFileDescriptor pidfd;

public:
	explicit SocketPeerAuth(SocketDescriptor s) noexcept;

	bool HaveCred() const noexcept {
		return cred.pid >= 0;
	}

	pid_t GetPid() const noexcept {
		assert(HaveCred());

		return cred.pid;
	}

	uid_t GetUid() const noexcept {
		assert(HaveCred());

		return cred.uid;
	}

	gid_t GetGid() const noexcept{
		assert(HaveCred());

		return cred.gid;
	}

	std::string_view GetCgroupPath() const;
};
