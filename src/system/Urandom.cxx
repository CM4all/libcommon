/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#include "Urandom.hxx"
#include "Error.hxx"

#ifdef HAVE_SYS_RANDOM_H
#include <sys/random.h>
#else
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/RuntimeError.hxx"

static std::size_t
Read(const char *path, FileDescriptor fd, void *p, std::size_t size)
{
	ssize_t nbytes = fd.Read(p, size);
	if (nbytes < 0)
		throw FormatErrno("Failed to read from %s", path);

	if (nbytes == 0)
		throw FormatRuntimeError("Short read from %s", path);

	return nbytes;
}

static void
FullRead(const char *path, FileDescriptor fd, void *_p, std::size_t size)
try {
	fd.FullRead(_p, size);
} catch (...) {
	std::throw_with_nested(FormatRuntimeError("Error on %s", path));
}

static std::size_t
Read(const char *path, void *p, std::size_t size)
{
	return Read(path, OpenReadOnly(path), p, size);
}

static void
FullRead(const char *path, void *p, std::size_t size)
{
	FullRead(path, OpenReadOnly(path), p, size);
}

#endif /* !HAVE_SYS_RANDOM_H */

std::size_t
UrandomRead(void *p, std::size_t size)
{
#ifdef HAVE_SYS_RANDOM_H
	ssize_t nbytes = getrandom(p, size, 0);
	if (nbytes < 0)
		throw MakeErrno("getrandom() failed");

	return nbytes;
#else
	return Read("/dev/urandom", p, size);
#endif
}

void
UrandomFill(void *p, std::size_t size)
{
#ifdef HAVE_SYS_RANDOM_H
	ssize_t nbytes = getrandom(p, size, 0);
	if (nbytes < 0)
		throw MakeErrno("getrandom() failed");

	if (std::size_t(nbytes) != size)
		throw std::runtime_error("getrandom() was incomplete");
#else
	FullRead("/dev/urandom", p, size);
#endif
}
