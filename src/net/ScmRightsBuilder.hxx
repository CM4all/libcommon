/*
 * Copyright 2007-2020 CM4all GmbH
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

#include <cassert>

#include <sys/socket.h>

template<size_t MAX_FDS>
class ScmRightsBuilder {
	static constexpr size_t size = CMSG_SPACE(MAX_FDS * sizeof(int));
	static constexpr size_t n_longs = (size + sizeof(long) - 1) / sizeof(long);

	size_t n = 0;
	long buffer[n_longs];

	int *data;

public:
	explicit ScmRightsBuilder(struct msghdr &msg) {
		msg.msg_control = buffer;
		msg.msg_controllen = sizeof(buffer);

		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		data = (int *)(void *)CMSG_DATA(cmsg);
	}

	void push_back(int fd) {
		assert(n < MAX_FDS);

		data[n++] = fd;
	}

	void Finish(struct msghdr &msg) {
		msg.msg_controllen = CMSG_SPACE(n * sizeof(int));

		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(n * sizeof(int));
	}
};
