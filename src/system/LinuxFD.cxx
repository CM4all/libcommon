/*
 * Copyright (C) 2012-2018 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
