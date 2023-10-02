// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <sys/types.h> // for off_t

class FileDescriptor;

/**
 * Copy all data from one file to the other.
 *
 * Throws on error.
 */
void
CopyRegularFile(FileDescriptor src, FileDescriptor dst, off_t size);
