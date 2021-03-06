/*
 * Copyright 2020-2021 CM4all GmbH
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

#include "CoOperation.hxx"
#include "Queue.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fcntl.h>

namespace Uring {

void
CoOperationBase::OnUringCompletion(int res) noexcept
{
	value = res;

	/* resume the coroutine which is co_awaiting the result (if
	   any) */
	if (continuation)
		continuation.resume();
}

UniqueFileDescriptor
CoOpenOperation::GetValue()
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to open file");

	return UniqueFileDescriptor(std::exchange(value, -1));
}

void
CoCloseOperation::GetValue()
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to close file");
}

CoCloseOperation
CoClose(Queue &queue, FileDescriptor fd) noexcept
{
	auto *s = queue.GetSubmitEntry();
	assert(s != nullptr); // TODO: what if the submit queue is full?

	io_uring_prep_close(s, fd.Get());

	CoCloseOperation op;
	queue.Push(*s, op);
	return op;
}

const struct statx &
CoStatxOperation::GetValue()
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to stat file");

	return stx;
}

inline
CoStatxOperation::CoStatxOperation(struct io_uring_sqe *s,
				   FileDescriptor directory_fd,
				   const char *path,
				   int flags, unsigned mask) noexcept
{
	io_uring_prep_statx(s, directory_fd.Get(), path,
			    flags, mask, &stx);
}

CoStatxOperation
CoStatx(Queue &queue,
	FileDescriptor directory_fd, const char *path,
	int flags, unsigned mask) noexcept
{
	auto *s = queue.GetSubmitEntry();
	assert(s != nullptr); // TODO: what if the submit queue is full?

	CoStatxOperation op(s, directory_fd, path, flags, mask);
	queue.Push(*s, op);
	return op;
}

static CoOpenOperation
CoOpen(Queue &queue, FileDescriptor directory_fd, const char *path,
       int flags, mode_t mode) noexcept
{
	auto *s = queue.GetSubmitEntry();
	assert(s != nullptr); // TODO: what if the submit queue is full?

	io_uring_prep_openat(s, directory_fd.Get(), path,
			     flags|O_NOCTTY|O_CLOEXEC, mode);

	CoOpenOperation op;
	queue.Push(*s, op);
	return op;
}

CoOpenOperation
CoOpenReadOnly(Queue &queue, FileDescriptor directory_fd, const char *path) noexcept
{
	return CoOpen(queue, directory_fd, path, O_RDONLY, 0);
}

CoOpenOperation
CoOpenReadOnly(Queue &queue, const char *path) noexcept
{
	return CoOpenReadOnly(queue, FileDescriptor(AT_FDCWD), path);
}

std::size_t
CoReadOperation::GetValue() const
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to read");

	return value;
}

CoReadOperation
CoRead(Queue &queue, FileDescriptor fd, void *buffer, std::size_t size,
       off_t offset, int flags) noexcept
{
	auto *s = queue.GetSubmitEntry();
	assert(s != nullptr); // TODO: what if the submit queue is full?

	io_uring_prep_read(s, fd.Get(), buffer, size, offset);
	s->flags = flags;

	CoReadOperation op;
	queue.Push(*s, op);
	return op;
}

std::size_t
CoWriteOperation::GetValue() const
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to write");

	return value;
}

CoWriteOperation
CoWrite(Queue &queue, FileDescriptor fd, const void *buffer, std::size_t size,
	off_t offset, int flags) noexcept
{
	auto *s = queue.GetSubmitEntry();
	assert(s != nullptr); // TODO: what if the submit queue is full?

	io_uring_prep_write(s, fd.Get(), buffer, size, offset);
	s->flags = flags;

	CoWriteOperation op;
	queue.Push(*s, op);
	return op;
}

} // namespace Uring
