// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "WriteBuffer.hxx"
#include "system/Error.hxx"

#include <assert.h>
#include <unistd.h>
#include <errno.h>

WriteBuffer::Result
WriteBuffer::Write(int fd)
{
	ssize_t nbytes = write(fd, buffer, GetSize());
	if (nbytes < 0) {
		switch (errno) {
		case EAGAIN:
		case EINTR:
			return Result::MORE;

		default:
			throw MakeErrno("Failed to write");
		}
	}

	buffer += nbytes;
	assert(buffer <= end);

	return buffer == end
		? Result::FINISHED
		: Result::MORE;
}
