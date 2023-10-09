// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "WriteBuffer.hxx"
#include "FileDescriptor.hxx"
#include "system/Error.hxx"

#include <assert.h>
#include <errno.h>

WriteBuffer::Result
WriteBuffer::Write(FileDescriptor fd)
{
	ssize_t nbytes = fd.Write({buffer, GetSize()});
	if (nbytes < 0) {
		switch (const int e = errno) {
		case EAGAIN:
		case EINTR:
			return Result::MORE;

		default:
			throw MakeErrno(e, "Failed to write");
		}
	}

	buffer += nbytes;
	assert(buffer <= end);

	return buffer == end
		? Result::FINISHED
		: Result::MORE;
}
