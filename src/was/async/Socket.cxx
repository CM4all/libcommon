// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Socket.hxx"
#include "system/Error.hxx"

#include <fcntl.h>
#include <sys/socket.h>

std::pair<WasSocket, WasSocket>
WasSocket::CreatePair()
{
	std::pair<WasSocket, WasSocket> result;

	if (!UniqueSocketDescriptor::CreateSocketPair(AF_LOCAL, SOCK_STREAM, 0,
						      result.first.control,
						      result.second.control))
		throw MakeErrno("Failed to create socket pair");

	if (!UniqueFileDescriptor::CreatePipe(result.first.input,
					      result.second.output))
		throw MakeErrno("Failed to create first pipe");

	if (!UniqueFileDescriptor::CreatePipe(result.second.input,
					      result.first.output))
		throw MakeErrno("Failed to create second pipe");

	/* allocate 256 kB for each pipe to reduce the system call and
	   latency overhead for splicing */
	static constexpr int PIPE_BUFFER_SIZE = 256 * 1024;
	result.first.input.SetPipeCapacity(PIPE_BUFFER_SIZE);
	result.first.output.SetPipeCapacity(PIPE_BUFFER_SIZE);

	return result;
}
