// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ProcFs.hxx"
#include "Mount.hxx"
#include "io/UniqueFileDescriptor.hxx"

UniqueFileDescriptor
CreateProcFilesystem()
{
	const auto fs = FSOpen("proc");
	FSConfig(fs, FSCONFIG_CMD_CREATE, nullptr, nullptr);
	return FSMount(fs, MS_NOEXEC|MS_NOSUID|MS_NODEV);
}
