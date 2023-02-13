// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "LinuxFD.hxx"
#include "Error.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/mman.h> //for memfd_create()

UniqueFileDescriptor
CreateEventFD(unsigned initval)
{
	int fd = eventfd(initval, EFD_NONBLOCK|EFD_CLOEXEC);
	if (fd < 0)
		throw MakeErrno("eventfd() failed");

	return UniqueFileDescriptor(fd);
}

UniqueFileDescriptor
CreateSignalFD(const sigset_t &mask, bool nonblock)
{
	int flags = SFD_CLOEXEC;
	if (nonblock)
		flags |= SFD_NONBLOCK;

	int fd = ::signalfd(-1, &mask, flags);
	if (fd < 0)
		throw MakeErrno("signalfd() failed");

	return UniqueFileDescriptor(fd);
}

UniqueFileDescriptor
CreateMemFD(const char *name, unsigned flags)
{
	flags |= MFD_CLOEXEC;

	int fd = memfd_create(name, flags);
	if (fd < 0)
		throw MakeErrno("memfd_create() failed");

	return UniqueFileDescriptor(fd);
}
