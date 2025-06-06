// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "MountNamespaceOptions.hxx"
#include "translation/Features.hxx"

#include <cstdint>

#include <sys/types.h> // for uid_t

class AllocatorPtr;
struct UidGid;
class MatchData;

struct NamespaceOptions {
	/**
	 * Start the child process in a new user namespace?
	 */
	bool enable_user = false;

	/**
	 * Start the child process in a new PID namespace?
	 */
	bool enable_pid = false;

	/**
	 * Start the child process in a new Cgroup namespace?
	 */
	bool enable_cgroup = false;

	/**
	 * Start the child process in a new network namespace?
	 */
	bool enable_network = false;

	/**
	 * Start the child process in a new IPC namespace?
	 */
	bool enable_ipc = false;

	/**
	 * The real uid visible to the spawned process.  If zero, then
	 * the original real uid is mapped.
	 */
	uid_t mapped_real_uid = 0;

	/**
	 * The effective uid visible to the spawned process.  If zero,
	 * then the original effective uid is mapped.
	 */
	uid_t mapped_effective_uid = 0;

	/**
	 * The name of the PID namespace to reassociate with.  The
	 * namespace is requested from the "Spawn" daemon (Package
	 * cm4all-spawn).
	 */
	const char *pid_namespace = nullptr;

	/**
	 * The name of the network namespace (/run/netns/X) to reassociate
	 * with.  Requires #enable_network.
	 */
	const char *network_namespace = nullptr;

	/**
	 * The hostname of the new UTS namespace.
	 */
	const char *hostname = nullptr;

	MountNamespaceOptions mount;

	NamespaceOptions() noexcept = default;

	constexpr NamespaceOptions(ShallowCopy shallow_copy,
				   const NamespaceOptions &src) noexcept
		:enable_user(src.enable_user),
		 enable_pid(src.enable_pid),
		 enable_cgroup(src.enable_cgroup),
		 enable_network(src.enable_network),
		 enable_ipc(src.enable_ipc),
		 mapped_real_uid(src.mapped_real_uid),
		 mapped_effective_uid(src.mapped_effective_uid),
		 pid_namespace(src.pid_namespace),
		 network_namespace(src.network_namespace),
		 hostname(src.hostname),
		 mount(shallow_copy, src.mount)
	{
	}

	NamespaceOptions(AllocatorPtr alloc, const NamespaceOptions &src) noexcept;

#if TRANSLATION_ENABLE_EXPAND
	[[gnu::pure]]
	bool IsExpandable() const noexcept;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Expand(AllocatorPtr alloc, const MatchData &match_data);
#endif

	/**
	 * Clear all network namespace options.
	 */
	void ClearNetwork() noexcept {
		enable_network = false;
		network_namespace = nullptr;
	}

	[[gnu::pure]]
	uint_least64_t GetCloneFlags(uint_least64_t flags) const noexcept;

	void SetupUidGidMap(const UidGid &uid_gid,
			    unsigned pid) const;

	/**
	 * Apply #pid_namespace.  This will affect new child
	 * processes, but not this process.
	 */
	void ReassociatePid() const;

	/**
	 * Apply #network_namespace.
	 */
	void ReassociateNetwork() const;

	/**
	 * Apply all options to the current process.  This assumes
	 * that GetCloneFlags() has been applied already.
	 *
	 * Throws std::system_error on error.
	 */
	void Apply(const UidGid &uid_gid) const;

	/**
	 * Apply only the network namespace options to the current
	 * process.  This can be done prior to clone() and Apply() to
	 * have those options in the parent process.  After that, you
	 * can call ClearNetwork() to avoid doing it again in the
	 * cloned child proces.
	 *
	 * Throws std::system_error on error.
	 */
	void ApplyNetwork() const;

	char *MakeId(char *p) const noexcept;
};
