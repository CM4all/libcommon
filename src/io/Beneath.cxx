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
	assert(file.directory.IsDefined());

	int fd = openat2(file.directory.Get(), file.name,
			 &ro_beneath, sizeof(ro_beneath));
	if (fd < 0)
		throw FmtErrno("Failed to open '{}'", file.name);

	return UniqueFileDescriptor{fd};
}