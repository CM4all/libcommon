// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Beneath.hxx"
#include "FileAt.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"

#include <cassert>

#include <fcntl.h>
#include <linux/openat2.h> // for struct open_how

static constexpr struct open_how ro_beneath{
	.flags = O_RDONLY|O_NOCTTY|O_CLOEXEC|O_NONBLOCK,
	.resolve = RESOLVE_BENEATH|RESOLVE_NO_MAGICLINKS,
};

UniqueFileDescriptor
TryOpenReadOnlyBeneath(FileAt file) noexcept
{
	assert(file.directory.IsDefined());

	return TryOpen(file, ro_beneath);
}

UniqueFileDescriptor
OpenReadOnlyBeneath(FileAt file)
{
	assert(file.directory.IsDefined());

	return Open(file, ro_beneath);
}

static constexpr struct open_how directory_beneath{
	.flags = O_DIRECTORY|O_RDONLY|O_NOCTTY|O_CLOEXEC|O_NONBLOCK,
	.resolve = RESOLVE_BENEATH|RESOLVE_NO_MAGICLINKS,
};

UniqueFileDescriptor
TryOpenDirectoryBeneath(FileAt file) noexcept
{
	assert(file.directory.IsDefined());

	return TryOpen(file, directory_beneath);
}

UniqueFileDescriptor
OpenDirectoryBeneath(FileAt file)
{
	assert(file.directory.IsDefined());

	return Open(file, directory_beneath);
}

static constexpr struct open_how path_beneath{
	.flags = O_PATH|O_CLOEXEC,
	.resolve = RESOLVE_BENEATH|RESOLVE_NO_MAGICLINKS,
};

UniqueFileDescriptor
TryOpenPathBeneath(FileAt file) noexcept
{
	assert(file.directory.IsDefined());

	return TryOpen(file, path_beneath);
}

UniqueFileDescriptor
OpenPathBeneath(FileAt file)
{
	assert(file.directory.IsDefined());

	return Open(file, path_beneath);
}
