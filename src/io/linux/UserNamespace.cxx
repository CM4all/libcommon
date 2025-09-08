// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "UserNamespace.hxx"
#include "ProcPid.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/WriteFile.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fmt/core.h>

#include <algorithm> // for std::copy()
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
char *
FormatIdMap(char *p, const IdMap::Item item) noexcept
{
	return fmt::format_to(p, "{} {} 1\n"sv, item.mapped_id, item.id);
}

[[nodiscard]]
static char *
FormatRootMap(char *p) noexcept
{
	constexpr std::string_view root_map = "0 0 1\n"sv;
	return std::copy(root_map.begin(), root_map.end(), p);
}

char *
FormatIdMap(char *p, const IdMap &map) noexcept
{
	p = FormatIdMap(p, map.first);
	if (map.second.id != 0 && map.second.id != map.first.id)
		p = FormatIdMap(p, map.second);

	if (map.root && map.first.id != 0)
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
SetupUidMap(unsigned pid, const IdMap &map)
{
	char buffer[256];
	char *end = FormatIdMap(buffer, map);

	WriteFileOrThrow(OpenProcPid(pid), "uid_map", {buffer, end});
}

void
SetupUidMap(unsigned pid, unsigned uid)
{
	char buffer[256];
	char *end = FormatIdMap(buffer, uid);

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
