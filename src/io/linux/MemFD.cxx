// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "MemFD.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <sys/mman.h> // for memfd_create()

UniqueFileDescriptor
CreateMemFD(const char *name, unsigned flags)
{
	flags |= MFD_CLOEXEC;

	int fd = memfd_create(name, flags);
	if (fd < 0)
		throw MakeErrno("memfd_create() failed");

	return UniqueFileDescriptor(AdoptTag{}, fd);
}
