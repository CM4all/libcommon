// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CoOperation.hxx"
#include "Queue.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fcntl.h>

namespace Uring {

void
CoOperationBase::OnUringCompletion(int res) noexcept
{
	result = res;

	/* resume the coroutine which is co_awaiting the result (if
	   any) */
	if (continuation)
		continuation.resume();
}

struct io_uring_sqe &
_RequireSubmitEntry(Queue &queue)
{
	return queue.RequireSubmitEntry();
}

void
_Push(Queue &queue, struct io_uring_sqe &sqe, Operation &operation) noexcept
{
	queue.Push(sqe, operation);
}

UniqueFileDescriptor
CoOpenOperation::GetValue(int value)
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to open file");

	return UniqueFileDescriptor(std::exchange(value, -1));
}

CoCloseOperation::CoCloseOperation(struct io_uring_sqe &s,
				   FileDescriptor fd) noexcept
{
	io_uring_prep_close(&s, fd.Get());
}

void
CoCloseOperation::GetValue(int value)
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to close file");
}

const struct statx &
CoStatxOperation::GetValue(int value)
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to stat file");

	return stx;
}

CoStatxOperation::CoStatxOperation(struct io_uring_sqe &s,
				   FileDescriptor directory_fd,
				   const char *path,
				   int flags, unsigned mask) noexcept
{
	io_uring_prep_statx(&s, directory_fd.Get(), path,
			    flags, mask, &stx);
}

CoOpenOperation::CoOpenOperation(struct io_uring_sqe &s,
				 FileDescriptor directory_fd, const char *path,
				 int flags, mode_t mode) noexcept
{
	io_uring_prep_openat(&s, directory_fd.Get(), path,
			     flags|O_NOCTTY|O_CLOEXEC|O_NONBLOCK, mode);
}

CoOperation<CoOpenOperation>
CoOpenReadOnly(Queue &queue, FileDescriptor directory_fd, const char *path) noexcept
{
	return {queue, directory_fd, path, O_RDONLY, 0};
}

CoOperation<CoOpenOperation>
CoOpenReadOnly(Queue &queue, const char *path) noexcept
{
	return CoOpenReadOnly(queue, FileDescriptor(AT_FDCWD), path);
}

CoReadOperation::CoReadOperation(struct io_uring_sqe &s,
				 FileDescriptor fd,
				 std::span<std::byte> dest,
				 off_t offset, int flags) noexcept
{
	io_uring_prep_read(&s, fd.Get(), dest.data(), dest.size(), offset);
	s.flags = flags;
}

std::size_t
CoReadOperation::GetValue(int value) const
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to read");

	return value;
}

CoBaseWriteOperation::CoBaseWriteOperation(struct io_uring_sqe &s,
					   FileDescriptor fd,
					   std::span<const std::byte> src,
					   off_t offset, int flags) noexcept
{
	io_uring_prep_write(&s, fd.Get(), src.data(), src.size(), offset);
	s.flags = flags;
}

std::size_t
CoWriteOperation::GetValue(int value) const
{
	if (value < 0)
		throw MakeErrno(-value, "Failed to write");

	return value;
}

CoUnlinkOperation::CoUnlinkOperation(struct io_uring_sqe &sqe,
				     const char *path,
				     int flags) noexcept
{
	io_uring_prep_unlink(&sqe, path, flags);
}

CoUnlinkOperation::CoUnlinkOperation(struct io_uring_sqe &sqe,
				     FileDescriptor directory_fd, const char *path,
				     int flags) noexcept
{
	io_uring_prep_unlinkat(&sqe, directory_fd.Get(), path, flags);
}

} // namespace Uring
