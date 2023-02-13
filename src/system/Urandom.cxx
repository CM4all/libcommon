// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
