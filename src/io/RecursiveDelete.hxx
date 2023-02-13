// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class FileDescriptor;

/**
 * Delete a file or directory recursively.
 *
 * Throws on error.
 */
void
RecursiveDelete(FileDescriptor parent, const char *filename);
