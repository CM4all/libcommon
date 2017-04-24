/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SPLICE_HXX
#define SPLICE_HXX

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

#endif
