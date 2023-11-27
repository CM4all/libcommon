// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Beneath.hxx"
#include "FileAt.hxx"
#include "UniqueFileDescriptor.hxx"
#include "system/openat2.h"
#include "lib/fmt/SystemError.hxx"

#include <cassert>

#include <fcntl.h>

static constexpr struct open_how ro_beneath{
	.flags = O_RDONLY|O_NOCTTY|O_CLOEXEC,
	.resolve = RESOLVE_BENEATH|RESOLVE_NO_MAGICLINKS,
};

UniqueFileDescriptor
TryOpenReadOnlyBeneath(FileAt file) noexcept
{
	assert(file.directory.IsDefined());

	int fd = openat2(file.directory.Get(), file.name,
			 &ro_beneath, sizeof(ro_beneath));
	return UniqueFileDescriptor{fd};
}

UniqueFileDescriptor
OpenReadOnlyBeneath(FileAt file)
{
	auto fd = TryOpenReadOnlyBeneath(file);
	if (!fd.IsDefined())
		throw FmtErrno("Failed to open '{}'", file.name);

	return fd;
}

static constexpr struct open_how directory_beneath{
	.flags = O_DIRECTORY|O_RDONLY|O_NOCTTY|O_CLOEXEC,
	.resolve = RESOLVE_BENEATH|RESOLVE_NO_MAGICLINKS,
};

UniqueFileDescriptor
TryOpenDirectoryBeneath(FileAt file) noexcept
{
	assert(file.directory.IsDefined());

	int fd = openat2(file.directory.Get(), file.name,
			 &directory_beneath, sizeof(directory_beneath));
	return UniqueFileDescriptor{fd};
}

UniqueFileDescriptor
OpenDirectoryBeneath(FileAt file)
{
	auto fd = TryOpenDirectoryBeneath(file);
	if (!fd.IsDefined())
		throw FmtErrno("Failed to open '{}'", file.name);

	return fd;
}
