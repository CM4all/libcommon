/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "SocketDescriptor.hxx"
#include "StaticSocketAddress.hxx"
#include "MsgHdr.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/ConstBuffer.hxx"

#include <vector>

#include <stdint.h>

template<size_t PAYLOAD_SIZE, size_t CMSG_SIZE>
struct ReceiveMessageBuffer {
	StaticSocketAddress address;

	uint8_t payload[PAYLOAD_SIZE];

	static constexpr size_t CMSG_BUFFER_SIZE = CMSG_SPACE(CMSG_SIZE);
	static constexpr size_t CMSG_N_LONGS = (CMSG_BUFFER_SIZE + sizeof(long) - 1) / sizeof(long);
	long cmsg[CMSG_N_LONGS];
};

struct ReceiveMessageResult {
	SocketAddress address;

	ConstBuffer<void> payload = nullptr;

	const struct ucred *cred = nullptr;

	std::vector<UniqueFileDescriptor> fds;
};

template<size_t PAYLOAD_SIZE, size_t CMSG_SIZE>
ReceiveMessageResult
ReceiveMessage(SocketDescriptor s,
	       ReceiveMessageBuffer<PAYLOAD_SIZE, CMSG_SIZE> &buffer,
	       int flags)
{
#ifdef MSG_CMSG_CLOEXEC
	/* implemented since Linux 2.6.23 */
	flags |= MSG_CMSG_CLOEXEC;
#endif

	struct iovec iov;
	iov.iov_base = buffer.payload;
	iov.iov_len = sizeof(buffer.payload);

	auto msg = MakeMsgHdr(buffer.address, {&iov, 1},
			      {buffer.cmsg, sizeof(buffer.cmsg)});

	auto nbytes = recvmsg(s.Get(), &msg, flags);
	if (nbytes < 0)
		throw MakeErrno("recvmsg() failed");

	if (nbytes == 0)
		return {};

	ReceiveMessageResult result;
	result.address = {buffer.address, msg.msg_namelen};
	result.payload = {buffer.payload, size_t(nbytes)};

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	while (cmsg != nullptr) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_CREDENTIALS) {
			result.cred = (const struct ucred *)CMSG_DATA(cmsg);
		} else if (cmsg->cmsg_level == SOL_SOCKET &&
			   cmsg->cmsg_type == SCM_RIGHTS) {
			const int *fds = (const int *)CMSG_DATA(cmsg);
			const size_t n = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(fds[0]);
			result.fds.reserve(result.fds.size() + n);

			for (size_t i = 0; i < n; ++i)
				result.fds.emplace_back(FileDescriptor(fds[i]));
		}

		cmsg = CMSG_NXTHDR(&msg, cmsg);
	}

#ifdef __clang__
#pragma GCC diagnostic pop
#endif

	return result;
}
