// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "UserNamespaceOptions.hxx"
#include "MountNamespaceOptions.hxx"
#include "PidNamespaceOptions.hxx"
#include "translation/Features.hxx"
#include "io/FileDescriptor.hxx"

#include <cstdint>

#include <sys/types.h> // for uid_t

class AllocatorPtr;
struct UidGid;
class MatchData;

struct NamespaceOptions {
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
	 * The name of the network namespace (/run/netns/X) to reassociate
	 * with.  Requires #enable_network.
	 */
	const char *network_namespace_name = nullptr;

	/**
	 * The hostname of the new UTS namespace.
	 */
	const char *hostname = nullptr;

	UserNamespaceOptions user;

	PidNamespaceOptions pid;

	MountNamespaceOptions mount;

	/**
	 * Namespace descriptors to reassociate with.
	 */
	FileDescriptor ipc_namespace = FileDescriptor::Undefined();

	NamespaceOptions() noexcept = default;

	constexpr NamespaceOptions(ShallowCopy shallow_copy,
				   const NamespaceOptions &src) noexcept
		:enable_cgroup(src.enable_cgroup),
		 enable_network(src.enable_network),
		 enable_ipc(src.enable_ipc),
		 network_namespace_name(src.network_namespace_name),
		 hostname(src.hostname),
		 user(shallow_copy, src.user),
		 pid(shallow_copy, src.pid),
		 mount(shallow_copy, src.mount),
		 ipc_namespace(src.ipc_namespace)
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
	 * Clear all pid namespace options.
	 */
	void ClearPid() noexcept {
		pid = {};
	}

	/**
	 * Clear all cgroup namespace options.
	 */
	void ClearCgroup() noexcept {
		enable_cgroup = false;
	}

	/**
	 * Clear all network namespace options.
	 */
	void ClearNetwork() noexcept {
		enable_network = false;
		network_namespace_name = nullptr;
	}

	/**
	 * Clear all IPC namespace options.
	 */
	void ClearIPC() noexcept {
		enable_ipc = false;
		ipc_namespace.SetUndefined();
	}

	[[gnu::pure]]
	uint_least64_t GetCloneFlags(uint_least64_t flags) const noexcept;

	/**
	 * Apply #network_namespace_name.
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
