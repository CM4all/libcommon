// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/StringBuffer.hxx"

#include <sys/types.h>

class FileDescriptor;

/**
 * Create a new directory with a random unique name.
 *
 * Throws on error.
 *
 * @return the name of the directory within the specified parent
 * directory
 */
StringBuffer<16>
MakeTempDirectory(FileDescriptor parent_fd, mode_t mode=0777);

/**
 * Move a file or directory to a random name within the new directory.
 *
 * Throws on error.
 *
 * @return the name of the directory within the specified new parent
 * directory
 */
StringBuffer<16>
MoveToTemp(FileDescriptor old_parent_fd, const char *old_name,
	   FileDescriptor new_parent_fd);
