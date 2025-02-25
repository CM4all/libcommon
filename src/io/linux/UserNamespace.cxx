// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "UserNamespace.hxx"
#include "ProcPid.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/WriteFile.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fmt/core.h>

#include <cassert>

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
		throw FmtErrno("write({:?}) failed", path);
}

void
SetupUidMap(unsigned pid, unsigned uid,
	    unsigned mapped_uid,
	    unsigned uid2,
	    bool root)
{
	char data_buffer[256], *p = data_buffer;

	p = fmt::format_to(p, "{} {} 1\n", mapped_uid, uid);
	if (uid2 != 0 && uid2 != uid)
		p = fmt::format_to(p, "{} {} 1\n", uid2, uid2);
	if (root && uid != 0)
		p = stpcpy(p, "0 0 1\n");

	WriteFileOrThrow(OpenProcPid(pid), "uid_map", {data_buffer, p});
}

void
SetupGidMap(unsigned pid, unsigned gid, bool root)
{
	char data_buffer[256], *p = data_buffer;

	p = fmt::format_to(p, "{} {} 1\n", gid, gid);
	if (root && gid != 0)
		p = stpcpy(p, "0 0 1\n");

	WriteFileOrThrow(OpenProcPid(pid), "gid_map", {data_buffer, p});
}

void
SetupGidMap(unsigned pid, const std::set<unsigned> &gids)
{
	assert(!gids.empty());

	char data_buffer[1024], *p = data_buffer;

	for (unsigned i : gids) {
		if (p + 64 > data_buffer + sizeof(data_buffer))
			break;

		p = fmt::format_to(p, "{} {} 1\n", i, i);
	}

	WriteFileOrThrow(OpenProcPid(pid), "gid_map", {data_buffer, p});
}
