// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MakeDirectory.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "lib/fmt/SystemError.hxx"
#include "util/ScopeExit.hxx"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

UniqueFileDescriptor
MakeDirectory(FileDescriptor parent_fd, const char *name,
	      const MakeDirectoryOptions options)
{
	if (mkdirat(parent_fd.Get(), name, options.mode) < 0) {
		const int e = errno;
		switch (e) {
		case EEXIST:
			break;

		default:
			throw FmtErrno(e, "Failed to create directory '{}'",
				       name);
		}
	}

	return OpenPath(parent_fd, name, O_DIRECTORY);
}

static char *
LastSlash(char *p, size_t size) noexcept
{
	/* chop trailing slashes off */
	while (size > 0 && p[size - 1] == '/')
		--size;

	return (char *)memrchr(p, '/', size);
}

static UniqueFileDescriptor
RecursiveMakeNestedDirectory(FileDescriptor parent_fd,
			     char *path, size_t path_length,
			     const MakeDirectoryOptions options)
{
	assert(path != nullptr);
	assert(path_length > 0);
	assert(path[path_length] == 0);

	if (mkdirat(parent_fd.Get(), path, options.mode) == 0)
		return OpenPath(parent_fd, path, O_DIRECTORY);

	const int e = errno;
	switch (e) {
	case EEXIST:
		return OpenPath(parent_fd, path, O_DIRECTORY);

	case ENOENT:
		/* parent directory doesn't exist - we must create it
		   first */
		break;

	default:
		throw FmtErrno(e, "Failed to create directory '{}'",
			       path);
	}

	char *slash = LastSlash(path, path_length);
	if (slash == nullptr)
		throw FmtErrno(e, "Failed to create directory '{}'",
			       path);

	*slash = 0;
	AtScopeExit(slash) { *slash = '/'; };

	char *name = slash + 1;
	while (*name == '/')
		++name;

	return MakeDirectory(RecursiveMakeNestedDirectory(parent_fd, path, slash - path,
							  options),
			     name,
			     options);
}

UniqueFileDescriptor
MakeNestedDirectory(FileDescriptor parent_fd, const char *path,
		    const MakeDirectoryOptions options)
{
	size_t path_length = strlen(path);
	char copy[PATH_MAX];
	if (path_length >= sizeof(copy))
		throw MakeErrno(ENAMETOOLONG, "Path too long");

	strcpy(copy, path);
	return RecursiveMakeNestedDirectory(parent_fd, copy, path_length,
					    options);
}
