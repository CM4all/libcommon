/*
 * Copyright 2014-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "MountInfo.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "lib/fmt/SystemError.hxx"
#include "util/ScopeExit.hxx"
#include "util/IterableSplitString.hxx"

#include <array>

using std::string_view_literals::operator""sv;

template<std::size_t N>
static void
SplitFill(std::array<std::string_view, N> &dest, std::string_view s, char separator)
{
	std::size_t i = 0;
	for (auto value : IterableSplitString(s, separator)) {
		dest[i++] = value;
		if (i >= dest.size())
			return;
	}

	std::fill(std::next(dest.begin(), i), dest.end(), std::string_view{});
}

static FILE *
Open(const char *path, const char *mode)
{
	FILE *file = fopen(path, mode);
	if (file == nullptr)
		throw FmtErrno("Failed to open {}", path);

	return file;
}

static FILE *
OpenMountInfo(unsigned pid)
{
	return Open(FmtBuffer<64>("/proc/{}/mountinfo", pid), "r");
}

MountInfo
ReadProcessMount(unsigned pid, const char *_mountpoint)
{
	FILE *const file = OpenMountInfo(pid);
	AtScopeExit(file) { fclose(file); };

	const std::string_view mountpoint(_mountpoint);

	char line[4096];
	while (fgets(line, sizeof(line), file) != nullptr) {
		std::array<std::string_view, 10> columns;
		SplitFill(columns, line, ' ');

		if (columns[4] == mountpoint) {
			/* skip the optional tagged fields */
			size_t i = 6;
			while (i < columns.size() && columns[i] != "-"sv)
				++i;

			if (i + 2 < columns.size())
				return MountInfo{
					std::string{columns[3]},
					std::string{columns[i + 1]},
					std::string{columns[i + 2]},
				};
		}
	}

	/* not found: return empty string */
	return {{}, {}, {}};
}
