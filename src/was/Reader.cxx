// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Reader.hxx"
#include "system/Error.hxx"

extern "C" {
#include <was/simple.h>
}

std::size_t
WasReader::Read(std::span<std::byte> dest)
{
	auto nbytes = was_simple_read(w, dest.data(), dest.size());
	if (nbytes < 0) {
		if (nbytes == -1)
			throw MakeErrno("Reading from request body failed");

		throw ReadFailed{};
	}

	return nbytes;
}
