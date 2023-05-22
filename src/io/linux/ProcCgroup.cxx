// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ProcCgroup.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "lib/fmt/SystemError.hxx"
#include "util/ScopeExit.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringSplit.hxx"

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
	const auto path = FmtBuffer<64>("/proc/{}/cgroup", pid);
	FILE *file = fopen(path, "r");
	if (file == nullptr)
		throw FmtErrno("Failed to open {}", path.c_str());

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
