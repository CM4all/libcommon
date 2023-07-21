// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "FileAt.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"

#include <concepts>

/**
 * Open the specified (regular) file read-only and pass it to the
 * given function.
 *
 * There are overloads which accept #FileAt (i.e. directory fd with
 * relative file name), just a path name (i.e. considered relative to
 * AT_FDCWD) and an already open #FileDescriptor.
 */
inline decltype(auto)
WithReadOnly(FileAt file_at, std::invocable<FileDescriptor> auto f)
{
	const auto fd = OpenReadOnly(file_at.directory, file_at.name);
	return f(FileDescriptor{fd});
}

inline decltype(auto)
WithReadOnly(const char *path, std::invocable<FileDescriptor> auto f)
{
	const auto fd = OpenReadOnly(path);
	return f(FileDescriptor{fd});
}

inline decltype(auto)
WithReadOnly(FileDescriptor fd, std::invocable<FileDescriptor> auto f)
{
	return f(fd);
}
