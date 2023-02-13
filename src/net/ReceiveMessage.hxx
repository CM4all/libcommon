// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SocketDescriptor.hxx"
#include "StaticSocketAddress.hxx"
#include "MsgHdr.hxx"
#include "SocketError.hxx"
#include "io/Iovec.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <span>
#include <vector>

#include <stdint.h>

template<size_t PAYLOAD_SIZE, size_t CMSG_SIZE>
struct ReceiveMessageBuffer {
	StaticSocketAddress address;

	std::byte payload[PAYLOAD_SIZE];

	static constexpr size_t CMSG_BUFFER_SIZE = CMSG_SPACE(CMSG_SIZE);
	static constexpr size_t CMSG_N_LONGS = (CMSG_BUFFER_SIZE + sizeof(long) - 1) / sizeof(long);
	long cmsg[CMSG_N_LONGS];
};

struct ReceiveMessageResult {
	SocketAddress address;

	std::span<const std::byte> payload{};

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

	struct iovec iov[] = {MakeIovec(buffer.payload)};

	auto msg = MakeMsgHdr(buffer.address, iov,
			      {(const std::byte *)buffer.cmsg, sizeof(buffer.cmsg)});

	auto nbytes = recvmsg(s.Get(), &msg, flags);
	if (nbytes < 0)
		throw MakeSocketError("recvmsg() failed");

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
