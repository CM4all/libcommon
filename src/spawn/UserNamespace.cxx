// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "UserNamespace.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/WriteFile.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/linux/ProcPid.hxx"

#include <assert.h>
#include <string.h>
#include <stdio.h>

void
DenySetGroups(unsigned pid)
try {
	TryWriteExistingFile(OpenProcPid(pid), "setgroups", "deny");
} catch (...) {
	// silently ignore errors
}

static void
WriteFileOrThrow(FileDescriptor directory, const char *path, std::string_view data)
{
	if (TryWriteExistingFile(directory, path, data) == WriteFileResult::ERROR)
		throw FmtErrno("write('{}') failed", path);
}

void
SetupUidMap(unsigned pid, unsigned uid, bool root)
{
	char data_buffer[256];

	size_t position = sprintf(data_buffer, "%u %u 1\n", uid, uid);
	if (root && uid != 0)
		strcpy(data_buffer + position, "0 0 1\n");

	WriteFileOrThrow(OpenProcPid(pid), "uid_map", data_buffer);
}

void
SetupGidMap(unsigned pid, unsigned gid, bool root)
{
	char data_buffer[256];

	size_t position = sprintf(data_buffer, "%u %u 1\n", gid, gid);
	if (root && gid != 0)
		strcpy(data_buffer + position, "0 0 1\n");

	WriteFileOrThrow(OpenProcPid(pid), "gid_map", data_buffer);
}

void
SetupGidMap(unsigned pid, const std::set<unsigned> &gids)
{
	assert(!gids.empty());

	char data_buffer[1024];

	size_t position = 0;
	for (unsigned i : gids) {
		if (position + 64 > sizeof(data_buffer))
			break;

		position += sprintf(data_buffer + position, "%u %u 1\n", i, i);
	}

	WriteFileOrThrow(OpenProcPid(pid), "gid_map", data_buffer);
}
