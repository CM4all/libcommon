// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "EasyMessage.hxx"
#include "ReceiveMessage.hxx"
#include "SendMessage.hxx"
#include "ScmRightsBuilder.hxx"
#include "SocketDescriptor.hxx"
#include "io/Iovec.hxx"

#include <cstdint>

void
EasySendMessage(SocketDescriptor s, std::span<const std::byte> payload,
		FileDescriptor fd)
{
	const struct iovec v[] = {MakeIovec(payload)};
	MessageHeader msg{v};

	ScmRightsBuilder<1> srb(msg);
	srb.push_back(fd.Get());
	srb.Finish(msg);

	SendMessage(s, msg, MSG_NOSIGNAL);
}

void
EasySendMessage(SocketDescriptor s, FileDescriptor fd)
{
	EasySendMessage(s, {}, fd);
}

UniqueFileDescriptor
EasyReceiveMessageWithOneFD(SocketDescriptor s)
{
	ReceiveMessageBuffer<1, 4> buffer;
	auto d = ReceiveMessage(s, buffer, 0);
	if (d.fds.empty())
		return {};

	return std::move(d.fds.front());
}
