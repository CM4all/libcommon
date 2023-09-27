// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SmallFile.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"

#include <sys/stat.h>

static void
ReadSmallFile(const char *path, FileDescriptor fd, std::span<std::byte> dest)
{
	struct stat st;
	if (fstat(fd.Get(), &st) < 0)
		throw FmtErrno("Failed to get file information about {}",
			       path);

	if (!S_ISREG(st.st_mode))
		throw FmtRuntimeError("{} is not a regular file", path);

	if (st.st_size != (off_t)dest.size())
		throw FmtRuntimeError("Size of {} is {}, should {}",
				      path, st.st_size,
				      dest.size());

	ssize_t nbytes = fd.Read(dest);
	if (nbytes < 0)
		throw FmtErrno("Failed to read from {}", path);

	if (size_t(nbytes) != dest.size())
		throw FmtRuntimeError("Short read from {}", path);
}

void
ReadSmallFile(const char *path, std::span<std::byte> dest)
{
	ReadSmallFile(path, OpenReadOnly(path), dest);
}
