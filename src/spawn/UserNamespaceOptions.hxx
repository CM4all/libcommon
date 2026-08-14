// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/FileDescriptor.hxx"
#include "util/TagStructs.hxx"

#include <cstdint>

#include <sys/types.h> // for uid_t

struct UidGid;
class AllocatorPtr;
class UniqueFileDescriptor;

struct UserNamespaceOptions {
	/**
	 * Namespace descriptors to reassociate with.
	 */
	FileDescriptor fd = FileDescriptor::Undefined();

	struct Limits {
		/**
		 * Maximum number of inotify instances (file
		 * `/proc/sys/user/max_inotify_instances`).
		 */
		uint_least32_t max_inotify_instances = UINT_LEAST32_MAX;

		/**
		 * Maximum number of inotify watches (file
		 * `/proc/sys/user/max_inotify_watches`).
		 */
		uint_least32_t max_inotify_watches = UINT_LEAST32_MAX;

		constexpr bool IsDefined() const noexcept {
			return max_inotify_instances != UINT_LEAST32_MAX ||
				max_inotify_watches != UINT_LEAST32_MAX;
		}

		/**
		 * Prepare a call to Apply().  This opens
		 * /proc/sys/user so Apply() may be called after /proc
		 * got unmounted.
		 */
		UniqueFileDescriptor PrepareApply() const;

		/**
		 * Apply all limit options to the user namespace of the
		 * current process.  This requires /proc being mounted.
		 *
		 * Throws on error.
		 */
		void Apply(FileDescriptor proc_sys_user) const;
	} limits;

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
	 * Start the child process in a new user namespace?
	 */
	bool create = false;

	[[nodiscard]]
	constexpr UserNamespaceOptions() noexcept = default;

	[[nodiscard]]
	constexpr UserNamespaceOptions([[maybe_unused]] ShallowCopy shallow_copy,
				      const UserNamespaceOptions &src) noexcept
		:fd(src.fd),
		 limits(src.limits),
		 mapped_real_uid(src.mapped_real_uid),
		 mapped_effective_uid(src.mapped_effective_uid),
		 create(src.create)
	{
	}

	[[nodiscard]]
	UserNamespaceOptions(AllocatorPtr alloc, const UserNamespaceOptions &src) noexcept;

	constexpr bool IsEnabled() const noexcept {
		return create || fd.IsDefined();
	}

	[[gnu::pure]]
	uint_least64_t GetCloneFlags(uint_least64_t flags) const noexcept;

	/**
	 * Generate a string for writing to /proc/PID/uid_map.  Use
	 * this method instead of SetupUidGidMap() if you want to use
	 * a custom way to create the user namespace.
	 */
	[[nodiscard]]
	char *FormatUidMap(char *dest, const UidGid &uid_gid) const noexcept;

	/**
	 * Generate a string for writing to /proc/PID/gid_map.  Use
	 * this method instead of SetupUidGidMap() if you want to use
	 * a custom way to create the user namespace.
	 */
	[[nodiscard]]
	char *FormatGidMap(char *dest, const UidGid &uid_gid) const noexcept;

	void SetupUidGidMap(const UidGid &uid_gid,
			    unsigned pid) const;

	char *MakeId(char *p) const noexcept;
};
