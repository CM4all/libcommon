// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

enum RecursiveCopyOptions : unsigned {
	/**
	 * Do not overwrite existing files.
	 */
	RECURSIVE_COPY_NO_OVERWRITE = 0x1U,

	/**
	 * Stay in the initial filesystem, don't cross mount points
	 * (like the `--one-file-system` option of `cp`).
	 *
	 * This is implemented by comparing the mount id.
	 */
	RECURSIVE_COPY_ONE_FILESYSTEM = 0x2U,

	/**
	 * Preserve file modes (permissions).
	 */
	RECURSIVE_COPY_PRESERVE_MODE = 0x4U,

	/**
	 * Preserve the modification time stamp.
	 */
	RECURSIVE_COPY_PRESERVE_TIME = 0x8U,
};

class FileDescriptor;

/**
 * Copies a file or directory recursively.
 * Symlinks are copied as-is, i.e. they are not rewritten.
 *
 * Throws on error.
 *
 * @param dst_filename the path within #dst_parent; if empty, copies
 * right into the given #dst_parent directory (only possible if the
 * source also refers to a directory)
 *
 * @param options one or more of #RecursiveCopyOptions
 */
void
RecursiveCopy(FileDescriptor src_parent, const char *src_filename,
	      FileDescriptor dst_parent, const char *dst_filename,
	      unsigned options=0);
