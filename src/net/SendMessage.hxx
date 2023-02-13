// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SocketAddress.hxx"

#include <sys/socket.h>

class SocketDescriptor;

/**
 * A convenience wrapper for struct msghdr.
 */
class MessageHeader : public msghdr {
public:
	constexpr MessageHeader(std::span<const struct iovec> payload) noexcept
		:msghdr{nullptr, 0,
			const_cast<struct iovec *>(payload.data()),
			payload.size(),
			nullptr, 0, 0} {}

	constexpr MessageHeader(std::span<struct iovec> payload) noexcept
		:MessageHeader(std::span<const struct iovec>{payload}) {}

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
