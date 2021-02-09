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

#include "SocketAddress.hxx"
#include "util/ConstBuffer.hxx"

#include <sys/socket.h>

template<typename T> struct ConstBuffer;
class SocketDescriptor;

/**
 * A convenience wrapper for struct msghdr.
 */
class MessageHeader : public msghdr {
public:
	constexpr MessageHeader(ConstBuffer<struct iovec> payload) noexcept
		:msghdr{nullptr, 0,
			const_cast<struct iovec *>(payload.data),
			payload.size,
			nullptr, 0, 0} {}

	MessageHeader &SetAddress(SocketAddress address) noexcept {
		msg_name = const_cast<struct sockaddr *>(address.GetAddress());
		msg_namelen = address.GetSize();
		return *this;
	}
};

/**
 * Wrapper for sendmsg().
 *
 * Throws on error.
 */
std::size_t
SendMessage(SocketDescriptor s, const MessageHeader &mh, int flags);
