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
