// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>

#include <sys/types.h>

class SocketDescriptor;
template<typename T> class ForeignFifoBuffer;

/**
 * Appends data from a socket to the buffer.
 *
 * @param s the source socket
 * @param buffer the destination buffer
 * @return -1 on error, -2 if the buffer is full, or the amount appended to the buffer
 */
ssize_t
ReceiveToBuffer(SocketDescriptor s,
		ForeignFifoBuffer<std::byte> &buffer) noexcept;

/**
 * Sends data from the buffer to the socket.
 *
 * @param s the destination socket
 * @param buffer the source buffer
 * @return -1 on error, -2 if the buffer is empty, or the number of
 * bytes sent
 */
ssize_t
SendFromBuffer(SocketDescriptor s,
	       ForeignFifoBuffer<std::byte> &buffer) noexcept;
