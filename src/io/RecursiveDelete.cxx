// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "RecursiveDelete.hxx"
#include "DirectoryReader.hxx"
#include "FileName.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "lib/fmt/SystemError.hxx"

#include <fcntl.h>
#include <unistd.h>

static void
ClearDirectory(UniqueFileDescriptor &&fd)
{
	DirectoryReader r{std::move(fd)};

	while (const char *child = r.Read())
		if (!IsSpecialFilename(child))
			RecursiveDelete(r.GetFileDescriptor(), child);
}

static void
RecursiveDeleteDirectory(FileDescriptor parent, const char *filename)
{
	ClearDirectory(OpenDirectory(parent, filename, O_NOFOLLOW));

	if (unlinkat(parent.Get(), filename, AT_REMOVEDIR) == 0)
		return;

	switch (const int e = errno; e) {
	case ENOENT:
		/* does not exist, nothing to do */
		return;

	default:
		throw FmtErrno(e, "Failed to delete {}", filename);
	}
}

void
RecursiveDelete(FileDescriptor parent, const char *filename)
{
	if (unlinkat(parent.Get(), filename, 0) == 0)
		return;

	switch (const int e = errno; e) {
	case EISDIR:
		/* switch to directory mode */
		RecursiveDeleteDirectory(parent, filename);
		return;

	case ENOENT:
		/* does not exist, nothing to do */
		return;

	default:
		throw FmtErrno(e, "Failed to delete {}", filename);
	}
}
