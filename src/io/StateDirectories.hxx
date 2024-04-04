// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <forward_list>
#include <span>

class FileDescriptor;
class UniqueFileDescriptor;

/**
 * Load state data from the following hard-coded base directories (in
 * this order, the first existing file is used):
 *
 * - /run/cm4all/state (temporary runtime state; does not persist reboots)
 * - /var/lib/cm4all/state (permanent runtime state; persists reboots)
 * - /etc/cm4all/state (locally configured state)
 * - /lib/cm4all/state (vendor state; from packages or from the OS image)
 *
 * Each setting is in a separate file (but files may be in arbitrary
 * subdirectories).
 *
 * This class is designed to be simple to use, and it is useful for
 * callers to use hard-coded relative paths.
 *
 * There is no error reporting, and callers are not supposed to handle
 * errors; if an error occurs, this class pretends the file simply
 * does not exist (but may log a message to stderr).  If the base
 * directories do not exist, nothing can ever be loaded, even if the
 * directories are created later (or moved).
 *
 * There is no code here to manage the directories or write files.
 * This is up to external tools.
 *
 * There is also no code for detecting changes.  Each software using
 * this class needs a way to tell the daemon process to reload from
 * this class.
 */
class StateDirectories {
	std::forward_list<UniqueFileDescriptor> directories;

public:
	StateDirectories() noexcept;
	~StateDirectories() noexcept;

	/**
	 * Locate a file and open it (read-only).  Returns an
	 * undefined UniqueFileDescriptor if the file does not exist
	 * in any of the base directories.
	 */
	[[gnu::pure]]
	UniqueFileDescriptor OpenFile(const char *relative_path) const noexcept;

	/**
	 * Load the contents of a file.
	 *
	 * @param buffer a caller-owned buffer (at most its size is being read)
	 * @return a pointer inside the caller-owned buffer with data
	 * read from the file or {nullptr,0} if the file does not exist
	 */
	[[gnu::pure]]
	std::span<const std::byte> GetBinary(const char *relative_path,
					     std::span<std::byte> buffer) const noexcept;

	[[gnu::pure]]
	signed GetSigned(const char *relative_path, int default_value) const noexcept;

	[[gnu::pure]]
	unsigned GetUnsigned(const char *relative_path, unsigned default_value) const noexcept;

	[[gnu::pure]]
	bool GetBool(const char *relative_path, bool default_value) const noexcept;

private:
	void AddDirectory(const char *path) noexcept;

	[[gnu::pure]]
	UniqueFileDescriptor OpenFileAutoFollow(const char *relative_path,
						unsigned follow_limit) const noexcept;

	UniqueFileDescriptor OpenFileFollow(FileDescriptor directory_fd,
					    const char *relative_path,
					    unsigned follow_limit) const noexcept;
};
