// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>
#include <string>

struct FileAt;

struct MountInfo {
	/**
	 * Unique identifier of the mount.
	 */
	uint_least64_t mnt_id;

	/**
	 * The relative path inside the file system which was mounted on
	 * the given mount point.  This is relevant for bind mounts.
	 */
	std::string root;

	/**
	 * The filesystem type.
	 */
	std::string filesystem;

	/**
	 * The device which was mounted on the given mount point.
	 */
	std::string source;

	bool IsDefined() const noexcept {
		return mnt_id != 0;
	}
};

/**
 * Determine which file system is mounted at the given mount point
 * path (exact match required).
 *
 * Throws std::runtime_error on error.
 *
 * @param pid a process id or 0 to obtain information about the
 * current process
 */
MountInfo
ReadProcessMount(unsigned pid, const char *mountpoint);

/**
 * Find a mounted device.
 *
 * Throws std::runtime_error on error.
 *
 * @param pid a process id or 0 to obtain information about the
 * current process
 *
 * @param major_minor a device specification using major and minor
 * number in the form "MAJOR:MINOR"
 */
MountInfo
FindMountInfoByDevice(unsigned pid, const char *major_minor);

/**
 * Find a mounted device by its id.
 *
 * Throws std::runtime_error on error.
 *
 * @param pid a process id or 0 to obtain information about the
 * current process
 *
 * @param mnt_id a mount id
 */
MountInfo
FindMountInfoById(unsigned pid, const uint_least64_t mnt_id);

/**
 * In which mount is the given path?
 *
 * Throws std::runtime_error on error.
 */
MountInfo
FindMountInfoByPath(FileAt path);

MountInfo
FindMountInfoByPath(const char *path);
