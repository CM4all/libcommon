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

#include "MultiReceiveMessage.hxx"
#include "SocketDescriptor.hxx"
#include "MsgHdr.hxx"
#include "SocketError.hxx"

#include <sys/socket.h>

MultiReceiveMessage::MultiReceiveMessage(size_t _allocated_datagrams,
					 size_t _max_payload_size,
					 size_t _max_cmsg_size,
					 size_t _max_fds)
	:allocated_datagrams(_allocated_datagrams),
	 max_payload_size(_max_payload_size), max_cmsg_size(_max_cmsg_size),
	 buffer(allocated_datagrams * (max_payload_size + max_cmsg_size
				       + sizeof(struct mmsghdr)
				       + sizeof(struct sockaddr_storage)
				       + sizeof(struct iovec))),
	 fds(_max_fds),
	 datagrams(new Datagram[allocated_datagrams])
{
	auto *m = GetMmsg();
	auto *v = (struct iovec *)(m + allocated_datagrams);
	auto *a = (struct sockaddr_storage *)(v + allocated_datagrams);

	/* initialize attributes which will not be modified by
	   recvmmsg() */
	for (size_t i = 0; i < allocated_datagrams; ++i) {
		v[i] = {GetPayload(i), max_payload_size};

		std::span<const std::byte> cmsg{};
		if (max_cmsg_size > 0)
			cmsg = {(const std::byte *)GetCmsg(i), max_cmsg_size};

		m[i].msg_hdr = MakeMsgHdr(*a, {v + i, 1}, cmsg);
	}
}

bool
MultiReceiveMessage::Receive(SocketDescriptor s)
{
	Clear();

	auto *m = GetMmsg();

	int flags = MSG_WAITFORONE;
#ifdef MSG_CMSG_CLOEXEC
	/* implemented since Linux 2.6.23 */
	flags |= MSG_CMSG_CLOEXEC;
#endif

	int result = recvmmsg(s.Get(), m, allocated_datagrams,
			      flags, nullptr);
	if (result == 0)
		return false;

	if (result < 0) {
		const auto code = GetSocketError();
		if (IsSocketErrorReceiveWouldBlock(code))
			return true;

		throw MakeSocketError(code, "recvmmsg() failed");
	}

	n_datagrams = result;

	auto *fds_p = fds.data();
	std::size_t remaining_fds = fds.size();

	for (size_t i = 0; i < n_datagrams; ++i) {
		auto &mh = m[i].msg_hdr;
		auto &d = datagrams[i];
		d.address = SocketAddress((const struct sockaddr *)mh.msg_name,
					  mh.msg_namelen);
		d.payload = {(std::byte *)GetPayload(i), m[i].msg_len};
		d.cred = nullptr;

		d.fds = {fds_p, 0};

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif

		for (auto *cmsg = CMSG_FIRSTHDR(&mh); cmsg != nullptr;
		     cmsg = CMSG_NXTHDR(&mh, cmsg)) {
			if (cmsg->cmsg_level == SOL_SOCKET &&
			    cmsg->cmsg_type == SCM_CREDENTIALS) {
				d.cred = (const struct ucred *)CMSG_DATA(cmsg);
			} else if (cmsg->cmsg_level == SOL_SOCKET &&
				   cmsg->cmsg_type == SCM_RIGHTS) {
				const int *f = (const int *)CMSG_DATA(cmsg);
				const unsigned nn = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(f[0]);

				for (unsigned ii = 0; ii < nn; ++ii) {
					FileDescriptor fd(f[ii]);
					if (remaining_fds > 0) {
						*fds_p++ = UniqueFileDescriptor(fd);
						--remaining_fds;
						d.fds = {d.fds.data(), d.fds.size() + 1};
					} else
						fd.Close();
				}
			}
		}

#ifdef __clang__
#pragma GCC diagnostic pop
#endif
	}

	return true;
}

void
MultiReceiveMessage::Clear()
{
	/* reinitialize attributes which are modified by recvmmsg */
	auto *m = GetMmsg();
	for (size_t i = 0; i < n_datagrams; ++i) {
		m[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_storage);
		m[i].msg_hdr.msg_controllen = max_cmsg_size;
	}

	n_datagrams = 0;

	while (n_fds > 0)
		fds[--n_fds].Close();
}
