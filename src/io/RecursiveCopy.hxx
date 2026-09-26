// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct RecursiveCopyOptions {
	/**
	 * Overwrite existing files?
	 */
	bool overwrite = true;

	/**
	 * Stay in the initial filesystem, don't cross mount points
	 * (like the `--one-file-system` option of `cp`).
	 *
	 * This is implemented by comparing the mount id.
	 */
	bool one_filesystem = false;

	/**
	 * Preserve file modes (permissions).
	 */
	bool preserve_mode = false;

	/**
	 * Preserve the modification time stamp.
	 */
	bool preserve_time = false;
};

class FileDescriptor;
struct FileAt;

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
RecursiveCopy(FileAt src_file, FileAt dst_file,
	      RecursiveCopyOptions options={});
