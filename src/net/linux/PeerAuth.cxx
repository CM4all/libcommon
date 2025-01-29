// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "PeerAuth.hxx"
#include "net/SocketDescriptor.hxx"
#include "io/linux/ProcCgroup.hxx"
#include "io/linux/ProcFdinfo.hxx"

#include <stdexcept>

#include <sys/socket.h> // for struct ucred

SocketPeerAuth::SocketPeerAuth(SocketDescriptor s) noexcept
	:cred(s.GetPeerCredentials()),
	 pidfd(s.GetPeerPidfd())
{
}

static std::string
ReadPidfdCgroup(FileDescriptor pidfd, pid_t expected_pid)
{
	const int pid = ReadPidfdPid(pidfd);
	if (pid < 0)
		throw std::runtime_error{"Client process has already exited"};

	/* must be the same PID as the one from SO_PEERCRED */
	if (expected_pid > 0 && expected_pid != pid)
		throw std::runtime_error{"PID mismatch"};

	/* read /proc/PID/cgroup */
	auto cgroup = ReadProcessCgroup(pid);

	/* verify the pidfd/PID again to rule out a race during
           ReadProcessCgroup() */
	if (const int pid2 = ReadPidfdPid(pidfd); pid2 < 0)
		throw std::runtime_error{"Client process has already exited"};
	else if (pid2 != pid)
		throw std::runtime_error{"PID mismatch"};

	return cgroup;
}

std::string_view
SocketPeerAuth::GetCgroupPath() const
{
	if (cgroup_path.empty()) {
		if (pidfd.IsDefined())
			cgroup_path = ReadPidfdCgroup(pidfd, cred.GetPid());
		else if (HaveCred())
			cgroup_path = ReadProcessCgroup(GetPid());
		else
			return {};
	}

	return cgroup_path;
}
