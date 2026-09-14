// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "NoSymlinks.hxx"
#include "FileAt.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"

#include <fcntl.h>
#include <linux/openat2.h> // for struct open_how

static constexpr struct open_how open_how_no_symlinks{
	.flags = O_PATH|O_NOFOLLOW|O_CLOEXEC,
	.resolve = RESOLVE_IN_ROOT|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_SYMLINKS,
};

static constexpr struct open_how open_how_directory_no_symlinks{
	.flags = O_PATH|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,
	.resolve = RESOLVE_IN_ROOT|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_SYMLINKS,
};

UniqueFileDescriptor
TryOpenPathNoSymlinks(FileAt file)
{
	return TryOpen(file, open_how_no_symlinks);
}

UniqueFileDescriptor
OpenPathNoSymlinks(FileAt file)
{
	return Open(file, open_how_no_symlinks);
}

UniqueFileDescriptor
TryOpenDirectoryPathNoSymlinks(FileAt file)
{
	return TryOpen(file, open_how_directory_no_symlinks);
}

UniqueFileDescriptor
OpenDirectoryPathNoSymlinks(FileAt file)
{
	return Open(file, open_how_directory_no_symlinks);
}
