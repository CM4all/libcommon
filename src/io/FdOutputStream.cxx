// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "FdOutputStream.hxx"
#include "system/Error.hxx"

#include <stdexcept>

void
FdOutputStream::Write(std::span<const std::byte> src)
{
	while (!src.empty()) {
		auto nbytes = fd.Write(src.data(), src.size());
		if (nbytes < 0)
			throw MakeErrno("Failed to write");

		if (nbytes == 0)
			throw std::runtime_error("Blocking write");

		src = src.subspan(nbytes);
	}
}
