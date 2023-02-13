// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "FdType.hxx"
#include "FileDescriptor.hxx"

#include <cassert>
#include <cstddef>

#include <fcntl.h>
#include <sys/sendfile.h>

static inline ssize_t
Splice(FileDescriptor src_fd, off64_t *src_offset,
       FileDescriptor dest_fd, off64_t *dest_offset,
       std::size_t max_length) noexcept
{
	assert(src_fd.IsDefined());
	assert(dest_fd.IsDefined());
	assert(src_fd != dest_fd);

	return splice(src_fd.Get(), src_offset, dest_fd.Get(), dest_offset,
		      max_length,
		      SPLICE_F_NONBLOCK | SPLICE_F_MOVE);
}

static inline ssize_t
SpliceToPipe(FileDescriptor src_fd, off64_t *src_offset,
	     FileDescriptor dest_fd,
	     std::size_t max_length) noexcept
{
	return Splice(src_fd, src_offset, dest_fd, nullptr, max_length);
}

static inline ssize_t
SpliceToSocket(FdType src_type, FileDescriptor src_fd, off64_t *src_offset,
	       FileDescriptor dest_fd, std::size_t max_length) noexcept
{
	assert(src_fd.IsDefined());
	assert(dest_fd.IsDefined());
	assert(src_fd != dest_fd);

	if (src_type == FdType::FD_PIPE) {
		return Splice(src_fd, src_offset, dest_fd, nullptr,
			      max_length);
	} else {
		assert(src_type == FdType::FD_FILE);

		return sendfile(dest_fd.Get(), src_fd.Get(), src_offset,
				max_length);
	}
}

static inline ssize_t
SpliceTo(FileDescriptor src_fd, FdType src_type, off64_t *src_offset,
	 FileDescriptor dest_fd, FdType dest_type,
	 std::size_t max_length) noexcept
{
	return IsAnySocket(dest_type)
		? SpliceToSocket(src_type, src_fd, src_offset, dest_fd, max_length)
		: SpliceToPipe(src_fd, src_offset, dest_fd, max_length);
}
