/*
 * Copyright 2007-2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "translation/Features.hxx"
#include "util/Compiler.h"
#include "util/IntrusiveForwardList.hxx"
#include "util/ShallowCopy.hxx"

class AllocatorPtr;
struct UidGid;
struct Mount;
class MatchInfo;

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

	const char *pivot_root = nullptr;

	const char *home = nullptr;

#if TRANSLATION_ENABLE_EXPAND
	const char *expand_home = nullptr;
#endif

	/**
	 * Mount the given home directory?  Value is the mount point.
	 */
	const char *mount_home = nullptr;

	/**
	 * Mount a new tmpfs on /tmp?  A non-empty string specifies
	 * additional mount options, such as "size=64M".
	 */
	const char *mount_tmp_tmpfs = nullptr;

	IntrusiveForwardList<Mount> mounts;

	MountNamespaceOptions() = default;

	constexpr MountNamespaceOptions(ShallowCopy shallow_copy,
					const MountNamespaceOptions &src) noexcept
		:mount_root_tmpfs(src.mount_root_tmpfs),
		 mount_proc(src.mount_proc),
		 writable_proc(src.writable_proc),
		 mount_pts(src.mount_pts),
		 bind_mount_pts(src.bind_mount_pts),
		 pivot_root(src.pivot_root),
		 home(src.home),
#if TRANSLATION_ENABLE_EXPAND
		 expand_home(src.expand_home),
#endif
		 mount_home(src.mount_home),
		 mount_tmp_tmpfs(src.mount_tmp_tmpfs),
		 mounts(shallow_copy, src.mounts) {}

	MountNamespaceOptions(AllocatorPtr alloc,
			      const MountNamespaceOptions &src) noexcept;

	bool IsEnabled() const noexcept {
		return mount_root_tmpfs || mount_proc ||
			mount_pts || bind_mount_pts ||
			pivot_root != nullptr ||
			mount_home != nullptr ||
			mount_tmp_tmpfs != nullptr ||
			!mounts.empty();
	}

#if TRANSLATION_ENABLE_EXPAND
	gcc_pure
	bool IsExpandable() const noexcept;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Expand(AllocatorPtr alloc, const MatchInfo &match_info);
#endif

	/**
	 * Apply all options to the current process.
	 *
	 * Throws std::system_error on error.
	 */
	void Apply(const UidGid &uid_gid) const;

	char *MakeId(char *p) const noexcept;

	const char *GetJailedHome() const noexcept {
		return mount_home != nullptr
			? mount_home
			: home;
	}

private:
	constexpr bool HasBindMount() const noexcept {
		return bind_mount_pts || mount_home != nullptr ||
			!mounts.empty();
	}
};
