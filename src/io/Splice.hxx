/*
 * Copyright 2007-2018 Content Management AG
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

#pragma once

#include "FdType.hxx"

#include <fcntl.h>
#include <sys/sendfile.h>

static inline ssize_t
Splice(int src_fd, int dest_fd, size_t max_length)
{
	assert(src_fd != dest_fd);

	return splice(src_fd, nullptr, dest_fd, nullptr, max_length,
		      SPLICE_F_NONBLOCK | SPLICE_F_MOVE);
}

static inline ssize_t
SpliceToPipe(int src_fd, int dest_fd, size_t max_length)
{
	assert(src_fd != dest_fd);

	return Splice(src_fd, dest_fd, max_length);
}

static inline ssize_t
SpliceToSocket(FdType src_type, int src_fd,
	       int dest_fd, size_t max_length)
{
	assert(src_fd != dest_fd);

	if (src_type == FdType::FD_PIPE) {
		return Splice(src_fd, dest_fd, max_length);
	} else {
		assert(src_type == FdType::FD_FILE);

		return sendfile(dest_fd, src_fd, nullptr, max_length);
	}
}

static inline ssize_t
SpliceTo(int src_fd, FdType src_type,
	 int dest_fd, FdType dest_type,
	 size_t max_length)
{
	return IsAnySocket(dest_type)
		? SpliceToSocket(src_type, src_fd, dest_fd, max_length)
		: SpliceToPipe(src_fd, dest_fd, max_length);
}
