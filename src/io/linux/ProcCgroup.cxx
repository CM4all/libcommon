// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ProcCgroup.hxx"
#include "ProcPid.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/ScopeExit.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringSplit.hxx"

static FILE *
OpenReadOnlyStdio(FileDescriptor directory, const char *path)
{
	auto fd = OpenReadOnly(directory, path);
	FILE *file = fdopen(fd.Get(), "r");
	if (file == nullptr)
		throw MakeErrno("fdopen() failed");
	fd.Release();
	return file;
}

static bool
ListContains(std::string_view haystack, char separator, std::string_view needle)
{
	for (const auto value : IterableSplitString(haystack, separator))
		if (value == needle)
			return true;

	return false;
}

static std::string_view
StripTrailingNewline(std::string_view s)
{
	return SplitWhile(s, [](char ch){ return ch != '\n'; }).first;
}

std::string
ReadProcessCgroup(unsigned pid, const std::string_view controller)
{
	FILE *file = OpenReadOnlyStdio(OpenProcPid(pid), "cgroup");
	AtScopeExit(file) { fclose(file); };

	char line[4096];
	while (fgets(line, sizeof(line), file) != nullptr) {
		const auto [controllers, group] = Split(Split(std::string_view{line}, ':').second, ':');
		if (!group.empty() && ListContains(controllers, ',', controller))
			return std::string{StripTrailingNewline(group)};
	}

	/* not found: return empty string */
	return {};
}
