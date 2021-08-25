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

#include "MultiClient.hxx"
#include "Socket.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "net/SendMessage.hxx"
#include "net/SocketDescriptor.hxx"
#include "io/Iovec.hxx"

#include <was/protocol.h>

namespace Was {

static constexpr struct was_header
MakeMultiWasHeader(enum multi_was_command cmd, std::size_t length) noexcept
{
	struct was_header h{};
	h.length = uint16_t(length);
	h.command = uint16_t(cmd);
	return h;
}

void
SendMultiNew(SocketDescriptor s, WasSocket &&socket)
{
	static constexpr auto header =
		MakeMultiWasHeader(MULTI_WAS_COMMAND_NEW, 0);
	auto vec = MakeIovecT(header);

	MessageHeader msg({&vec, 1});

	ScmRightsBuilder<3> b(msg);
	b.push_back(socket.control.Get());
	b.push_back(socket.input.Get());
	b.push_back(socket.output.Get());
	b.Finish(msg);

	SendMessage(s, msg, MSG_NOSIGNAL);
}

} // namespace Was
