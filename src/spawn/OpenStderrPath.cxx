// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "OpenStderrPath.hxx"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fcntl.h>

UniqueFileDescriptor
OpenStderrPath(const char *path)
{
	assert(path != nullptr);

	static constexpr struct open_how how{
		.flags = O_CREAT|O_WRONLY|O_APPEND|O_NOCTTY|O_CLOEXEC|O_NONBLOCK,
		.mode = 0600,
	};

	return Open({FileDescriptor{AT_FDCWD}, path}, how);
}
