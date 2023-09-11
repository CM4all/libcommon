// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ProcCgroup.hxx"
#include "ProcPid.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/ScopeExit.hxx"
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

static std::string_view
StripTrailingNewline(std::string_view s)
{
	return SplitWhile(s, [](char ch){ return ch != '\n'; }).first;
}

std::string
ReadProcessCgroup(unsigned pid)
{
	FILE *file = OpenReadOnlyStdio(OpenProcPid(pid), "cgroup");
	AtScopeExit(file) { fclose(file); };

	char line[4096];
	while (fgets(line, sizeof(line), file) != nullptr) {
		const auto [controllers, group] = Split(Split(std::string_view{line}, ':').second, ':');
		if (controllers.empty() && !group.empty())
			return std::string{StripTrailingNewline(group)};
	}

	/* not found: return empty string */
	return {};
}
