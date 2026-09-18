// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

class FileDescriptor;
struct FileAt;

struct RecursiveDeleteOptions {
	/**
	 * The maximum directory nesting depth (to avoid stack
	 * overflows).
	 */
	unsigned max_depth = 128;
};

/**
 * Delete a file or directory recursively.
 *
 * Throws on error.
 */
void
RecursiveDelete(FileAt file, RecursiveDeleteOptions options={});
