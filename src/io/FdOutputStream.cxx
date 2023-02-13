// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "FdOutputStream.hxx"
#include "system/Error.hxx"
#include "util/OffsetPointer.hxx"

#include <stdexcept>

void
FdOutputStream::Write(const void *data, size_t size)
{
	while (size > 0) {
		auto nbytes = fd.Write(data, size);
		if (nbytes < 0)
			throw MakeErrno("Failed to write");

		if (nbytes == 0)
			throw std::runtime_error("Blocking write");

		data = OffsetPointer(data, nbytes);
		size -= nbytes;
	}
}
