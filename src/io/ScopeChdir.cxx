// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ScopeChdir.hxx"
#include "Open.hxx"
#include "system/Error.hxx"

#include <fcntl.h> // for O_DIRECTORY
#include <unistd.h> // for fchdir(), chdir()

ScopeChdir::ScopeChdir()
	:old(OpenPath(".", O_DIRECTORY))
{
}

ScopeChdir::ScopeChdir(const char *path)
	:ScopeChdir()
{
	if (chdir(path) < 0)
		throw FormatErrno("Failed to change to directory %s", path);
}

ScopeChdir::ScopeChdir(FileDescriptor new_wd)
	:ScopeChdir()
{
	if (fchdir(new_wd.Get()) < 0)
		throw MakeErrno("Failed to change directory");
}

ScopeChdir::~ScopeChdir() noexcept
{
	[[maybe_unused]]
	int dummy = fchdir(old.Get());
}
