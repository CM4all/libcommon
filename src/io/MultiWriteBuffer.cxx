// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MultiWriteBuffer.hxx"
#include "FileDescriptor.hxx"
#include "Iovec.hxx"
#include "system/Error.hxx"

#include <sys/uio.h>
#include <errno.h>

MultiWriteBuffer::Result
MultiWriteBuffer::Write(FileDescriptor fd)
{
	assert(i < n);

	std::array<iovec, MAX_BUFFERS> iov;

	for (unsigned k = i; k != n; ++k)
		iov[k] = MakeIovec(buffers[k]);

	ssize_t nbytes = writev(fd.Get(), &iov[i], n - i);
	if (nbytes < 0) {
		switch (errno) {
		case EAGAIN:
		case EINTR:
			return Result::MORE;

		default:
			throw MakeErrno("Failed to write");
		}
	}

	while (i != n) {
		WriteBuffer &b = buffers[i];
		if (size_t(nbytes) < b.GetSize()) {
			b.buffer += nbytes;
			return Result::MORE;
		}

		nbytes -= b.GetSize();
		++i;
	}

	return Result::FINISHED;
}
