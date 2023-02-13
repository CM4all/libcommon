// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "RecursiveDelete.hxx"
#include "DirectoryReader.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "system/Error.hxx"

#include <fcntl.h>
#include <unistd.h>

[[gnu::pure]]
static bool
IsSpecialFilename(const char *s) noexcept
{
    return s[0]=='.' && (s[1] == 0 || (s[1] == '.' && s[2] == 0));
}

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
		throw FormatErrno(e, "Failed to delete %s", filename);
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
		throw FormatErrno(e, "Failed to delete %s", filename);
	}
}
