// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Urandom.hxx"
#include "Error.hxx"

#include <sys/random.h>

std::size_t
UrandomRead(void *p, std::size_t size)
{
	ssize_t nbytes = getrandom(p, size, 0);
	if (nbytes < 0)
		throw MakeErrno("getrandom() failed");

	return nbytes;
}

void
UrandomFill(void *p, std::size_t size)
{
	ssize_t nbytes = getrandom(p, size, 0);
	if (nbytes < 0)
		throw MakeErrno("getrandom() failed");

	if (std::size_t(nbytes) != size)
		throw std::runtime_error("getrandom() was incomplete");
}
