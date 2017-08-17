/*
 * Copyright 2007-2017 Content Management AG
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

#include "MultiWriteBuffer.hxx"
#include "system/Error.hxx"

#include <sys/uio.h>
#include <errno.h>

MultiWriteBuffer::Result
MultiWriteBuffer::Write(int fd)
{
    assert(i < n);

    std::array<iovec, MAX_BUFFERS> iov;

    for (unsigned k = i; k != n; ++k) {
        iov[k].iov_base = const_cast<void *>(buffers[k].GetData());
        iov[k].iov_len = buffers[k].GetSize();
    };

    ssize_t nbytes = writev(fd, &iov[i], n - i);
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
