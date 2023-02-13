// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cassert>
#include <cstddef>

#include <sys/socket.h>

/**
 * This class helps with sending file descriptors over a local socket.
 * To use it, first construct a #msghdr and then an instance of this
 * class.  Call push_back() for each file descriptor, and finally call
 * Finish().  After that, sendmsg() can be called.
 *
 * @param MAX_FDS the maximum number of file descriptors
 */
template<std::size_t MAX_FDS>
class ScmRightsBuilder {
	static constexpr std::size_t size = CMSG_SPACE(MAX_FDS * sizeof(int));
	static constexpr std::size_t n_longs = (size + sizeof(long) - 1) / sizeof(long);

	std::size_t n = 0;
	long buffer[n_longs];

	int *data;

public:
	explicit ScmRightsBuilder(struct msghdr &msg) noexcept {
		msg.msg_control = buffer;
		msg.msg_controllen = sizeof(buffer);

		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		data = (int *)(void *)CMSG_DATA(cmsg);
	}

	void push_back(int fd) noexcept {
		assert(n < MAX_FDS);

		data[n++] = fd;
	}

	void Finish(struct msghdr &msg) noexcept {
		msg.msg_controllen = CMSG_SPACE(n * sizeof(int));

		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(n * sizeof(int));
	}
};
