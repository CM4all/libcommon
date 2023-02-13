// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string_view>

class FileDescriptor;

enum class WriteFileResult {
	/**
	 * The operation was successful.
	 */
	SUCCESS,

	/**
	 * There was an I/O error, and errno contains details.
	 */
	ERROR,

	/**
	 * The write operation was too short - not all bytes were
	 * written.
	 */
	SHORT,
};

/**
 * Attempt to write a string to the given file.  It must already
 * exist, and it is not truncated or appended.  This function is
 * useful to write "special" files like the ones in /proc.
 */
WriteFileResult
TryWriteExistingFile(const char *path, std::string_view value) noexcept;

#ifdef __linux__

/**
 * Attempt to write a string to the given file.  It must already
 * exist, and it is not truncated or appended.  This function is
 * useful to write "special" files like the ones in /proc.
 */
WriteFileResult
TryWriteExistingFile(FileDescriptor directory, const char *path,
		     std::string_view value) noexcept;

#endif
