/*
 * Copyright 2021-2022 CM4all GmbH
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

#include "SmallFile.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"

#include <sys/stat.h>

static void
ReadSmallFile(const char *path, FileDescriptor fd, std::span<std::byte> dest)
{
	struct stat st;
	if (fstat(fd.Get(), &st) < 0)
		throw FormatErrno("Failed to get file information about %s",
				  path);

	if (!S_ISREG(st.st_mode))
		throw FormatRuntimeError("%s is not a regular file", path);

	if (st.st_size != (off_t)dest.size())
		throw FormatRuntimeError("Size of %s is %llu, should %Zu",
					 path, (unsigned long long)st.st_size,
					 dest.size());

	ssize_t nbytes = fd.Read(dest.data(), dest.size());
	if (nbytes < 0)
		throw FormatErrno("Failed to read from %s", path);

	if (size_t(nbytes) != dest.size())
		throw FormatRuntimeError("Short read from %s", path);
}

void
ReadSmallFile(const char *path, std::span<std::byte> dest)
{
	ReadSmallFile(path, OpenReadOnly(path), dest);
}
