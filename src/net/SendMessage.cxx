// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SendMessage.hxx"
#include "SocketDescriptor.hxx"
#include "SocketError.hxx"

std::size_t
SendMessage(SocketDescriptor s, const MessageHeader &mh, int flags)
{
	auto nbytes = s.Send(mh, flags);
	if (nbytes < 0)
		throw MakeSocketError("sendmsg() failed");

	return (std::size_t)nbytes;
}
