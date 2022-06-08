/*
 * Copyright 2020-2022 CM4all GmbH
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

	auto read = CoRead(queue, fd, value.data(), size, 0,
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
