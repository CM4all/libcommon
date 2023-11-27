// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef MAKE_DIRECTORY_HXX
#define MAKE_DIRECTORY_HXX

#include <sys/types.h>

class FileDescriptor;
class UniqueFileDescriptor;

struct MakeDirectoryOptions {
	mode_t mode = 0777;

	/**
	 * Throw an error if the directory already exists?
	 */
	bool exclusive = false;
};

/**
 * Open a directory, and create it if it does not exist.
 *
 * @param mode the mode parameter for the mkdir() call; it has no
 * effect if the directory already exists
 * @return an O_PATH file handle to the directory
 */
UniqueFileDescriptor
MakeDirectory(FileDescriptor parent_fd, const char *name,
	      MakeDirectoryOptions options=MakeDirectoryOptions{});

/**
 * Like MakeDirectory(), but create parent directories as well.
 *
 * @param path a relative path which may contain segments separated by
 * slash; however, it must not contain "." and ".." segments
 */
UniqueFileDescriptor
MakeNestedDirectory(FileDescriptor parent_fd, const char *path,
		    MakeDirectoryOptions options=MakeDirectoryOptions{});

#endif
