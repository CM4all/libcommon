// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

struct FileAt;
class UniqueFileDescriptor;

/**
 * Open the specified file as an O_PATH descriptor, but don't follow
 * any symlinks while resolving the given path.
 */
UniqueFileDescriptor
TryOpenPathNoSymlinks(FileAt file);

/**
 * Like TryOpenPathNoSymlinks(), but throw on error.
 */
UniqueFileDescriptor
OpenPathNoSymlinks(FileAt file);

/**
 * Open the specified directory as an O_PATH descriptor, but don't
 * follow any symlinks while resolving the given path.
 */
UniqueFileDescriptor
TryOpenDirectoryPathNoSymlinks(FileAt file);

/**
 * Like TryOpenDirectoryPathNoSymlinks(), but throw on error.
 */
UniqueFileDescriptor
OpenDirectoryPathNoSymlinks(FileAt file);
