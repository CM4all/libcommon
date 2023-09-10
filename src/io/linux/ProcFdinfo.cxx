// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ProcFdinfo.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "io/SmallTextFile.hxx"
#include "util/NumberParser.hxx"
#include "util/StringCompare.hxx" // for SkipPrefix()

#include <cassert>
#include <stdexcept>

using std::string_view_literals::operator""sv;

unsigned
ReadPidfdPid(FileDescriptor pidfd)
{
	assert(pidfd.IsDefined());

	std::optional<unsigned> result;

	ForEachTextLine<4096>(FmtBuffer<64>("/proc/self/fdinfo/{}", pidfd.Get()).c_str(), [&result](std::string_view line){
		if (SkipPrefix(line, "Pid:\t"sv)) {
			auto pid = ParseInteger<int>(line);
			if (!pid)
				throw std::runtime_error{"Failed to parse Pid line"};

			result = *pid;
		}
	});

	if (!result)
		throw std::runtime_error{"Not a pidfd"};

	return *result;
}
