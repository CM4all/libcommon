// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupEvents.hxx"
#include "io/SmallTextFile.hxx"
#include "util/NumberParser.hxx"

using std::string_view_literals::operator""sv;

CgroupMemoryEvents
ReadCgroupMemoryEvents(FileDescriptor fd)
{
	CgroupMemoryEvents result{};

	for (const std::string_view line : IterableSmallTextFile<4096>{fd}) {
		const auto [name, value] = Split(line, ' ');
		if (name == "oom_kill"sv)
			ParseIntegerTo(value, result.oom_kill);
	}

	return result;
}

CgroupPidsEvents
ReadCgroupPidsEvents(FileDescriptor fd)
{
	CgroupPidsEvents result{};

	for (const std::string_view line : IterableSmallTextFile<4096>{fd}) {
		const auto [name, value] = Split(line, ' ');
		if (name == "max"sv)
			ParseIntegerTo(value, result.max);
	}

	return result;
}
