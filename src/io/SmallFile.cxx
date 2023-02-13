// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
