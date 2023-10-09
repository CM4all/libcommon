// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Urandom.hxx"
#include "Error.hxx"

#include <sys/random.h>

std::size_t
UrandomRead(std::span<std::byte> dest)
{
	ssize_t nbytes = getrandom(dest.data(), dest.size(), 0);
	if (nbytes < 0)
		throw MakeErrno("getrandom() failed");

	return nbytes;
}

void
UrandomFill(std::span<std::byte> dest)
{
	std::size_t nbytes = UrandomRead(dest);
	if (nbytes != dest.size())
		throw std::runtime_error("getrandom() was incomplete");
}
