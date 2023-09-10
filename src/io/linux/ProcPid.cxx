// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ProcPid.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fcntl.h> // for O_DIRECTORY

UniqueFileDescriptor
OpenProcPid(unsigned pid)
{
	return pid > 0
		? OpenPath(FmtBuffer<64>("/proc/{}", pid), O_DIRECTORY|O_NOFOLLOW)
		: OpenPath("/proc/self", O_DIRECTORY);
}
