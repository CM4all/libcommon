// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "RecursiveDelete.hxx"
#include "DirectoryReader.hxx"
#include "FileAt.hxx"
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
			RecursiveDelete({r.GetFileDescriptor(), child});
}

static void
RecursiveDeleteDirectory(FileAt file)
{
	ClearDirectory(OpenDirectory(file, O_NOFOLLOW));

	if (unlinkat(file.directory.Get(), file.name, AT_REMOVEDIR) == 0)
		return;

	switch (const int e = errno; e) {
	case ENOENT:
		/* does not exist, nothing to do */
		return;

	default:
		throw FmtErrno(e, "Failed to delete {}", file.name);
	}
}

void
RecursiveDelete(FileAt file)
{
	if (unlinkat(file.directory.Get(), file.name, 0) == 0)
		return;

	switch (const int e = errno; e) {
	case EISDIR:
		/* switch to directory mode */
		RecursiveDeleteDirectory(file);
		return;

	case ENOENT:
		/* does not exist, nothing to do */
		return;

	default:
		throw FmtErrno(e, "Failed to delete {}", file.name);
	}
}
