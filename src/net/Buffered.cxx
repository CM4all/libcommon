/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Buffered.hxx"
#include "util/ForeignFifoBuffer.hxx"

#include <assert.h>
#include <sys/socket.h>
#include <errno.h>

ssize_t
ReceiveToBuffer(int fd, ForeignFifoBuffer<uint8_t> &buffer)
{
    assert(fd >= 0);

    auto w = buffer.Write();
    if (w.IsEmpty())
        return -2;

    ssize_t nbytes = recv(fd, w.data, w.size, MSG_DONTWAIT);
    if (nbytes > 0)
        buffer.Append((size_t)nbytes);

    return nbytes;
}

ssize_t
SendFromBuffer(int fd, ForeignFifoBuffer<uint8_t> &buffer)
{
    auto r = buffer.Read();
    if (r.IsEmpty())
        return -2;

    ssize_t nbytes = send(fd, r.data, r.size, MSG_DONTWAIT|MSG_NOSIGNAL);
    if (nbytes >= 0)
        buffer.Consume((size_t)nbytes);
    else if (errno == EAGAIN)
        nbytes = 0;

    return nbytes;
}
