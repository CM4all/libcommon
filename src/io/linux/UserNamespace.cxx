// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "UserNamespace.hxx"
#include "io/FileAt.hxx"
#include "io/WriteFile.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fmt/format.h>

#include <algorithm> // for std::copy()
#include <cassert>

using std::string_view_literals::operator""sv;

void
DenySetGroups(FileDescriptor proc_pid) noexcept
{
	// silently ignore errors
	(void)TryWriteExistingFile({proc_pid, "setgroups"}, "deny");
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

void
SetupUidMap(FileDescriptor proc_pid, const IdMap &map)
{
	char buffer[256];
	char *end = FormatIdMap(buffer, map);

	WriteExistingFile({proc_pid, "uid_map"}, {buffer, end});
}

void
SetupUidMap(FileDescriptor proc_pid, unsigned uid)
{
	char buffer[256];
	char *end = FormatIdMap(buffer, uid);

	WriteExistingFile({proc_pid, "uid_map"}, {buffer, end});
}

void
SetupGidMap(FileDescriptor proc_pid, unsigned gid)
{
	char buffer[256];
	char *end = FormatIdMap(buffer, gid);

	WriteExistingFile({proc_pid, "gid_map"}, {buffer, end});
}

void
SetupGidMap(FileDescriptor proc_pid, const std::set<unsigned> &gids)
{
	assert(!gids.empty());

	char buffer[1024];
	char *end = FormatIdMap(buffer, gids);

	WriteExistingFile({proc_pid, "gid_map"}, {buffer, end});
}
