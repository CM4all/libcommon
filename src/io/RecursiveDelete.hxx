// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

class FileDescriptor;
struct FileAt;

/**
 * Delete a file or directory recursively.
 *
 * Throws on error.
 */
void
RecursiveDelete(FileAt file);
