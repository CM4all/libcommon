// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "translation/Features.hxx"
#include "util/IntrusiveForwardList.hxx"
#include "util/TagStructs.hxx"

#include <cstddef>
#include <cstdint>
#include <span>

class AllocatorPtr;
struct UidGid;
struct Mount;
class MatchData;

struct MountNamespaceOptions {
	/**
	 * Mount a tmpfs to "/"?  All required mountpoints will be
	 * created, but the filesystem will contain nothing else.
	 */
	bool mount_root_tmpfs = false;

	/**
	 * Mount a new /proc?
	 */
	bool mount_proc = false;

	/**
	 * Shall /proc we writable?  Only used if #mount_proc is set.
	 */
	bool writable_proc = false;

	/**
	 * Mount /dev?
	 */
	bool mount_dev = false;

	/**
	 * Mount /dev/pts?
	 */
	bool mount_pts = false;

	/**
	 * Bind-mount the old /dev/pts?
	 *
	 * Note that #Mount cannot be used here because it enforces
	 * MS_NODEV.
	 */
	bool bind_mount_pts = false;

#if TRANSLATION_ENABLE_EXPAND
	bool expand_home = false;
#endif

	const char *pivot_root = nullptr;

	const char *home = nullptr;

	/**
	 * Mount a new tmpfs on /tmp?  A non-empty string specifies
	 * additional mount options, such as "size=64M".
	 */
	const char *mount_tmp_tmpfs = nullptr;

	/**
	 * @see TranslationCommand::MOUNT_LISTEN_STREAM
	 *
	 * Note that this field is not used by the spawner library.
	 * The calling application must evaluate it, set up the
	 * listener and set up a bind mount.
	 */
	std::span<const std::byte> mount_listen_stream;

	IntrusiveForwardList<Mount> mounts;

	/**
	 * The mode for directories created in tmpfs.
	 */
	uint_least16_t dir_mode = 0711;

	MountNamespaceOptions() = default;

	constexpr MountNamespaceOptions(ShallowCopy shallow_copy,
					const MountNamespaceOptions &src) noexcept
		:mount_root_tmpfs(src.mount_root_tmpfs),
		 mount_proc(src.mount_proc),
		 writable_proc(src.writable_proc),
		 mount_dev(src.mount_dev),
		 mount_pts(src.mount_pts),
		 bind_mount_pts(src.bind_mount_pts),
#if TRANSLATION_ENABLE_EXPAND
		 expand_home(src.expand_home),
#endif
		 pivot_root(src.pivot_root),
		 home(src.home),
		 mount_tmp_tmpfs(src.mount_tmp_tmpfs),
		 mount_listen_stream(src.mount_listen_stream),
		 mounts(shallow_copy, src.mounts),
		 dir_mode(src.dir_mode) {}

	MountNamespaceOptions(AllocatorPtr alloc,
			      const MountNamespaceOptions &src) noexcept;

	/**
	 * Is something mounted on the filesystem root, i.e. is
	 * pivot_root() going to be called?
	 */
	bool IsRootMounted() const noexcept {
		return mount_root_tmpfs || pivot_root != nullptr;
	}

	bool IsEnabled() const noexcept {
		return IsRootMounted() || mount_proc || mount_dev ||
			mount_pts || bind_mount_pts ||
			mount_tmp_tmpfs != nullptr ||
			mount_listen_stream.data() != nullptr ||
			!mounts.empty();
	}

#if TRANSLATION_ENABLE_EXPAND
	[[gnu::pure]]
	bool IsExpandable() const noexcept;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Expand(AllocatorPtr alloc, const MatchData &match_data);
#endif

	/**
	 * Apply all options to the current process.
	 *
	 * Throws std::system_error on error.
	 */
	void Apply(const UidGid &uid_gid) const;

	char *MakeId(char *p) const noexcept;

	[[gnu::pure]]
	bool HasMountOn(const char *target) const noexcept;

	/**
	 * Translate a host path to a path inside this mount
	 * namespace.  Returns nullptr if no matching bind mount was
	 * found.  May return #host_path (i.e. the string must remain
	 * valid as long as the return value is used).
	 */
	[[gnu::pure]]
	const char *ToContainerPath(AllocatorPtr alloc,
				    const char *host_path) const noexcept;

private:
	constexpr bool HasBindMount() const noexcept {
		return bind_mount_pts || !mounts.empty();
	}

	[[gnu::pure]]
	std::pair<const Mount *, const char *> FindBindMountInSource(const char *source) const noexcept;
};
