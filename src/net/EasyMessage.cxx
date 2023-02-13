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
EasySendMessage(SocketDescriptor s, FileDescriptor fd)
{
	static uint8_t dummy_payload = 0;
	const struct iovec v[] = {MakeIovecT(dummy_payload)};
	MessageHeader msg{v};

	ScmRightsBuilder<1> srb(msg);
	srb.push_back(fd.Get());
	srb.Finish(msg);

	SendMessage(s, msg, MSG_NOSIGNAL);
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
