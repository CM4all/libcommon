// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "TmpfsCreate.hxx"
#include "system/Mount.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fmt/format.h>

UniqueFileDescriptor
CreateTmpfs(bool exec)
{
	unsigned flags = MOUNT_ATTR_NOSUID|MOUNT_ATTR_NODEV;
	if (!exec)
		flags |= MOUNT_ATTR_NOEXEC;

	auto fs = FSOpen("tmpfs");
	FSConfig(fs, FSCONFIG_SET_STRING, "size", "64M");
	FSConfig(fs, FSCONFIG_SET_STRING, "nr_inodes", "65536");
	FSConfig(fs, FSCONFIG_SET_STRING, "mode", "1777");
	FSConfig(fs, FSCONFIG_CMD_CREATE, nullptr, nullptr);

	return FSMount(fs, flags);
}
