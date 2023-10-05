// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "FdReader.hxx"
#include "system/Error.hxx"

#include <assert.h>

std::size_t
FdReader::Read(std::span<std::byte> dest)
{
	assert(fd.IsDefined());

	const auto nbytes = fd.Read(dest);
	if (nbytes < 0)
		throw MakeErrno("Failed to read");

	return nbytes;
}
