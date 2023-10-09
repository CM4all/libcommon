// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CoTextFile.hxx"
#include "CoOperation.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <liburing.h>

#include <stdexcept>

#include <fcntl.h>

namespace Uring {

static Co::Task<size_t>
RegularFileSize(Queue &queue, FileDescriptor fd)
{
	auto s = CoStatx(queue, fd, "", AT_EMPTY_PATH,
			 STATX_TYPE|STATX_SIZE);
	const auto &stx = co_await s;

	if (!S_ISREG(stx.stx_mode))
		throw std::runtime_error("Not a regular file");

	co_return stx.stx_size;
}

Co::Task<std::string>
CoReadTextFile(Queue &queue, FileDescriptor directory_fd, const char *path,
	       size_t max_size)
{
	auto fd = co_await CoOpenReadOnly(queue, directory_fd, path);
	const auto size = co_await RegularFileSize(queue, fd);
	if (size > max_size)
		throw std::runtime_error("File is too large");

	std::string value;
	value.resize(size);

	/* hard-link the read() and the close() with IOSQE_IO_HARDLINK
	   (requires Linux 5.6) */

	auto read = CoRead(queue, fd,
			   std::as_writable_bytes(std::span{value.data(), size}),
			   0,
			   IOSQE_IO_HARDLINK);

	try {
		co_await CoClose(queue, fd);
		fd.Steal();
	} catch (...) {
		/* if CoClose() fails, ~UniqueFileDescriptor() fall
		   back to regular close() */
	}

	const auto nbytes = co_await read;
	if (nbytes != size)
		throw std::runtime_error("Short read");

	co_return value;
}

} // namespace Uring
