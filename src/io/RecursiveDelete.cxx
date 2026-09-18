// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "RecursiveDelete.hxx"
#include "DirectoryReader.hxx"
#include "FileAt.hxx"
#include "FileName.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"

#include <fcntl.h>
#include <unistd.h>

static void
RecursiveDelete(FileAt file, unsigned remaining_depth);

static void
ClearDirectory(UniqueFileDescriptor &&fd, unsigned remaining_depth)
{
	DirectoryReader r{std::move(fd)};

	while (const char *child = r.Read())
		if (!IsSpecialFilename(child))
			RecursiveDelete({r.GetFileDescriptor(), child},
					remaining_depth);
}

static void
RecursiveDeleteDirectory(FileAt file, unsigned remaining_depth)
{
	if (remaining_depth == 0)
		throw std::runtime_error{"Directory hierarchy is too deep"};

	ClearDirectory(OpenDirectory(file, O_NOFOLLOW), remaining_depth - 1);

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

static void
RecursiveDelete(FileAt file, unsigned remaining_depth)
{
	if (unlinkat(file.directory.Get(), file.name, 0) == 0)
		return;

	switch (const int e = errno; e) {
	case EISDIR:
		/* switch to directory mode */
		RecursiveDeleteDirectory(file, remaining_depth);
		return;

	case ENOENT:
		/* does not exist, nothing to do */
		return;

	default:
		throw FmtErrno(e, "Failed to delete {}", file.name);
	}
}

void
RecursiveDelete(FileAt file, RecursiveDeleteOptions options)
{
	RecursiveDelete(file, options.max_depth);
}
