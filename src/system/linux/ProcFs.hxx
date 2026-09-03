// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

class UniqueFileDescriptor;

/**
 * Create a new "proc" filesystem superblock and return it as a
 * directory file descriptor.
 *
 * Throws on error.
 */
UniqueFileDescriptor
CreateProcFilesystem();
