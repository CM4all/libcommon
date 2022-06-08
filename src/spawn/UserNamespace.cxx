/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "UserNamespace.hxx"
#include "system/Error.hxx"
#include "io/WriteFile.hxx"

#include <assert.h>
#include <string.h>
#include <stdio.h>

void
DenySetGroups()
{
	TryWriteExistingFile("/proc/self/setgroups", "deny");
}

static void
WriteFileOrThrow(const char *path, std::string_view data)
{
	if (TryWriteExistingFile(path, data) == WriteFileResult::ERROR)
		throw FormatErrno("write('%s') failed", path);
}

void
SetupUidMap(unsigned pid, unsigned uid, bool root)
{
	char path_buffer[64], data_buffer[256];

	const char *path = "/proc/self/uid_map";
	if (pid > 0) {
		sprintf(path_buffer, "/proc/%u/uid_map", pid);
		path = path_buffer;
	}

	size_t position = sprintf(data_buffer, "%u %u 1\n", uid, uid);
	if (root && uid != 0)
		strcpy(data_buffer + position, "0 0 1\n");

	WriteFileOrThrow(path, data_buffer);
}

void
SetupGidMap(unsigned pid, unsigned gid, bool root)
{
	char path_buffer[64], data_buffer[256];

	const char *path = "/proc/self/gid_map";
	if (pid > 0) {
		sprintf(path_buffer, "/proc/%u/gid_map", pid);
		path = path_buffer;
	}

	size_t position = sprintf(data_buffer, "%u %u 1\n", gid, gid);
	if (root && gid != 0)
		strcpy(data_buffer + position, "0 0 1\n");

	WriteFileOrThrow(path, data_buffer);
}

void
SetupGidMap(unsigned pid, const std::set<unsigned> &gids)
{
	assert(!gids.empty());

	char path_buffer[64], data_buffer[1024];

	const char *path = "/proc/self/gid_map";
	if (pid > 0) {
		sprintf(path_buffer, "/proc/%u/gid_map", pid);
		path = path_buffer;
	}

	size_t position = 0;
	for (unsigned i : gids) {
		if (position + 64 > sizeof(data_buffer))
			break;

		position += sprintf(data_buffer + position, "%u %u 1\n", i, i);
	}

	WriteFileOrThrow(path, data_buffer);
}
