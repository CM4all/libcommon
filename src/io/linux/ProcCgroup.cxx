// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ProcCgroup.hxx"
#include "ProcPid.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/SmallTextFile.hxx"

std::string
ReadProcessCgroup(unsigned pid)
{
	for (const std::string_view line : IterableSmallTextFile<8192>(FileAt{OpenProcPid(pid), "cgroup"})) {
		const auto [controllers, group] = Split(Split(std::string_view{line}, ':').second, ':');
		if (controllers.empty() && !group.empty())
			return std::string{group};
	}

	/* not found: return empty string */
	return {};
}
