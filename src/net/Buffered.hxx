/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef NET_BUFFERED_HXX
#define NET_BUFFERED_HXX

#include <sys/types.h>
#include <stdint.h>

template<typename T> class ForeignFifoBuffer;

/**
 * Appends data from a socket to the buffer.
 *
 * @param fd the source socket
 * @param buffer the destination buffer
 * @return -1 on error, -2 if the buffer is full, or the amount appended to the buffer
 */
ssize_t
ReceiveToBuffer(int fd, ForeignFifoBuffer<uint8_t> &buffer);

/**
 * Sends data from the buffer to the socket.
 *
 * @param fd the destination socket
 * @param buffer the source buffer
 * @return -1 on error, -2 if the buffer is empty, or the number of
 * bytes sent
 */
ssize_t
SendFromBuffer(int fd, ForeignFifoBuffer<uint8_t> &buffer);

#endif
