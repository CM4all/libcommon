// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

/**
 * This class helps with building a new VFS (virtual file system).  It
 * remembers which paths have a writable "tmpfs" and creates mount
 * points inside it.
 */
class VfsBuilder {
	struct Item;

	std::vector<Item> items;

	const uint_least16_t dir_mode;

	int old_umask = -1;

public:
	const uint_least32_t uid, gid;

	/**
	 * @param _dir_mode the "mode" parameter to mkdir() when
	 * directories in tmpfs are created
	 */
	[[nodiscard]]
	VfsBuilder(uint_least32_t _uid, uint_least32_t _gid, uint_least16_t _dir_mode) noexcept;

	~VfsBuilder() noexcept;

	uint_least16_t GetDirMode() const noexcept {
		return dir_mode;
	}

	void AddWritableRoot(const char *path);

	/**
	 * Throws if the mount point could not be created.
	 */
	void Add(std::string_view path);

	/**
	 * Throws if the mount point could not be opened.
	 */
	void MakeWritable();

	/**
	 * Schedule a remount of the most recently added mount point.
	 */
	void ScheduleRemount(uint_least64_t attr_set,
			     uint_least64_t attr_clr) noexcept;

	/**
	 * Make sure the specified directory exists inside a writable
	 * mount.  Throws if that fails.  Returns false if the mount
	 * point above the given path is not writable.
	 */
	bool MakeOptionalDirectory(std::string_view path);

	/**
	 * Make sure the specified directory exists inside a writable
	 * mount.  Throws if that fails.
	 */
	void MakeDirectory(std::string_view path);

	void Finish();

private:
	struct FindWritableResult;

	[[gnu::pure]]
	FindWritableResult FindWritable(std::string_view path) const noexcept;
};
