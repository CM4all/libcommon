// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <type_traits>
#include <span>

/**
 * Read the contents of a regular file to the given buffer.
 *
 * Throws on I/O error or if the file is not a regular file or if the
 * file has a different size.
 */
void
ReadSmallFile(const char *path, std::span<std::byte> dest);

template<typename T>
requires std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
void
ReadSmallFile(const char *path, std::span<T> dest)
{
	ReadSmallFile(path, std::as_writable_bytes(dest));
}

template<typename T>
T
ReadSmallFile(const char *path)
{
	T value;
	ReadSmallFile(path, std::span{&value, 1U});
	return value;
}
