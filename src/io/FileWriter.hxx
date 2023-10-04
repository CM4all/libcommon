// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "UniqueFileDescriptor.hxx"

#include <cstddef>
#include <span>
#include <string>

/**
 * Create a new file or replace an existing file.  This class attempts
 * to make this atomic: if an error occurs, it attempts to restore the
 * previous state.
 */
class FileWriter {
	std::string path;

	std::string tmp_path;

	FileDescriptor directory_fd;

	UniqueFileDescriptor fd;

public:
	FileWriter() = default;

	/**
	 * Throws std::system_error on error.
	 */
	explicit FileWriter(const char *_path, mode_t mode=0666);

	FileWriter(FileDescriptor _directory_fd, const char *_path,
		   mode_t mode=0666);

	~FileWriter() noexcept {
		if (fd.IsDefined())
			Cancel();
	}

	FileWriter(FileWriter &&src) noexcept = default;

	FileWriter &operator=(FileWriter &&src) noexcept {
		if (IsDefined())
			Cancel();

		path = std::move(src.path);
		tmp_path = std::move(src.tmp_path);
		directory_fd = src.directory_fd;
		fd = std::move(src.fd);
		return *this;
	}

	bool IsDefined() const noexcept {
		return fd.IsDefined();
	}

	FileDescriptor GetFileDescriptor() noexcept {
		return fd;
	}

	/**
	 * Attempt to allocate space on the file system.  This may
	 * speed up following writes, and may reduce file system
	 * fragmentation.  This is a hint, and there is no error
	 * checking.
	 */
	void Allocate(off_t size) noexcept;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Write(std::span<const std::byte> src);

	/**
	 * Throws std::system_error on error.
	 */
	void Commit();

	/**
	 * Attempt to undo the changes by this class.
	 */
	void Cancel() noexcept;
};
