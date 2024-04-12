// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MakeDirectory.hxx"
#include "UniqueFileDescriptor.hxx"
#include "lib/fmt/SystemError.hxx"
#include "system/linux/openat2.h"
#include "util/ScopeExit.hxx"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

static constexpr int
FilterErrno(int e, const MakeDirectoryOptions options) noexcept
{
	if (e == EEXIST && !options.exclusive)
		e = 0;

	return e;
}

static UniqueFileDescriptor
OpenDirectory(FileDescriptor directory, const char *name,
	      const MakeDirectoryOptions options)
{
	struct open_how how{
		.flags = O_DIRECTORY|O_PATH|O_RDONLY|O_CLOEXEC,
	};

	if (!options.follow_symlinks) {
		how.flags |= O_NOFOLLOW;
		how.resolve |= RESOLVE_NO_SYMLINKS;
	}

	int fd = openat2(directory.Get(), name, &how, sizeof(how));
	if (fd < 0)
		throw FmtErrno("Failed to open {:?}", name);

	return UniqueFileDescriptor{fd};
}

UniqueFileDescriptor
MakeDirectory(FileDescriptor parent_fd, const char *name,
	      const MakeDirectoryOptions options)
{
	if (mkdirat(parent_fd.Get(), name, options.mode) < 0) {
		switch (const int e = FilterErrno(errno, options)) {
		case 0:
			break;

		default:
			throw FmtErrno(e, "Failed to create directory {:?}",
				       name);
		}
	}

	return OpenDirectory(parent_fd, name, options);
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
		return OpenDirectory(parent_fd, path, options);

	const int e = FilterErrno(errno, options);
	switch (e) {
	case 0:
		return OpenDirectory(parent_fd, path, options);

	case ENOENT:
		/* parent directory doesn't exist - we must create it
		   first */
		break;

	default:
		throw FmtErrno(e, "Failed to create directory {:?}",
			       path);
	}

	char *slash = LastSlash(path, path_length);
	if (slash == nullptr)
		throw FmtErrno(e, "Failed to create directory {:?}",
			       path);

	*slash = 0;
	AtScopeExit(slash) { *slash = '/'; };

	char *name = slash + 1;
	while (*name == '/')
		++name;

	auto middle_options = options;
	middle_options.exclusive = false;

	return MakeDirectory(RecursiveMakeNestedDirectory(parent_fd, path, slash - path,
							  middle_options),
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
