// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ProcPid.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fcntl.h> // for O_NOFOLLOW

UniqueFileDescriptor
OpenProcPid(unsigned pid)
{
	return pid > 0
		? OpenDirectoryPath({FileDescriptor::Undefined(), FmtBuffer<64>("/proc/{}", pid)}, O_NOFOLLOW)
		: OpenDirectoryPath({FileDescriptor::Undefined(), "/proc/self"});
}
