// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "OpenStderrPath.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fcntl.h>

using std::string_view_literals::operator""sv;

UniqueFileDescriptor
OpenStderrPath(const char *path)
{
	assert(path != nullptr);

	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_CREAT|O_WRONLY|O_APPEND, 0600))
		throw FmtErrno("open({:?}) failed"sv, path);

	return fd;
}
