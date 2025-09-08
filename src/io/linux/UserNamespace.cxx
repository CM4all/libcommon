// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "UserNamespace.hxx"
#include "ProcPid.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/WriteFile.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fmt/core.h>

#include <cassert>

using std::string_view_literals::operator""sv;

void
DenySetGroups(unsigned pid) noexcept
try {
	TryWriteExistingFile(OpenProcPid(pid), "setgroups", "deny");
} catch (...) {
	// silently ignore errors
}

[[nodiscard]]
static char *
FormatIdMap(char *p, unsigned id, unsigned mapped_id) noexcept
{
	return fmt::format_to(p, "{} {} 1\n"sv, mapped_id, id);
}

[[nodiscard]]
static char *
FormatIdMap(char *p, unsigned id) noexcept
{
	return FormatIdMap(p, id, id);
}

[[nodiscard]]
static char *
FormatRootMap(char *p) noexcept
{
	return stpcpy(p, "0 0 1\n");
}

char *
FormatIdMap(char *p, unsigned id,
	    unsigned mapped_id,
	    unsigned id2, unsigned mapped_id2,
	    bool root) noexcept
{
	p = FormatIdMap(p, id, mapped_id);
	if (id2 != 0 && id2 != id)
		p = FormatIdMap(p, id2, mapped_id2);
	if (root && id != 0)
		p = FormatRootMap(p);

	return p;
}

char *
FormatIdMap(char *p, const std::set<unsigned> &ids) noexcept
{
	for (unsigned id : ids)
		p = FormatIdMap(p, id);

	return p;
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
	    unsigned uid2, unsigned mapped_uid2,
	    bool root)
{
	char buffer[256];
	char *end = FormatIdMap(buffer, uid, mapped_uid, uid2, mapped_uid2, root);

	WriteFileOrThrow(OpenProcPid(pid), "uid_map", {buffer, end});
}

void
SetupGidMap(unsigned pid, unsigned gid)
{
	char buffer[256];
	char *end = FormatIdMap(buffer, gid);

	WriteFileOrThrow(OpenProcPid(pid), "gid_map", {buffer, end});
}

void
SetupGidMap(unsigned pid, const std::set<unsigned> &gids)
{
	assert(!gids.empty());

	char buffer[1024];
	char *end = FormatIdMap(buffer, gids);

	WriteFileOrThrow(OpenProcPid(pid), "gid_map", {buffer, end});
}
