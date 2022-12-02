/*
 * Copyright 2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FileAt.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "util/Concepts.hxx"

/**
 * Open the specified (regular) file read-only and pass it to the
 * given function.
 *
 * There are overloads which accept #FileAt (i.e. directory fd with
 * relative file name), just a path name (i.e. considered relative to
 * AT_FDCWD) and an already open #FileDescriptor.
 */
inline decltype(auto)
WithReadOnly(FileAt file_at, Invocable<FileDescriptor> auto f)
{
	const auto fd = OpenReadOnly(file_at.directory, file_at.name);
	return f(FileDescriptor{fd});
}

inline decltype(auto)
WithReadOnly(const char *path, Invocable<FileDescriptor> auto f)
{
	const auto fd = OpenReadOnly(path);
	return f(FileDescriptor{fd});
}

inline decltype(auto)
WithReadOnly(FileDescriptor fd, Invocable<FileDescriptor> auto f)
{
	return f(fd);
}
