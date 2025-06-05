// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

class FileDescriptor;

/**
 * Delete a file or directory recursively.
 *
 * Throws on error.
 */
void
RecursiveDelete(FileDescriptor parent, const char *filename);
