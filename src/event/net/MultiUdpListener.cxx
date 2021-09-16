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

#include "MultiUdpListener.hxx"
#include "UdpHandler.hxx"
#include "system/Error.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/Compiler.h"

#include <assert.h>

MultiUdpListener::MultiUdpListener(EventLoop &event_loop,
				   UniqueSocketDescriptor _socket,
				   MultiReceiveMessage &&_multi,
				   UdpHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(EventCallback), _socket.Release()),
	 multi(std::move(_multi)),
	 handler(_handler)
{
	event.ScheduleRead();
}

MultiUdpListener::~MultiUdpListener() noexcept
{
	event.Close();
}

void
MultiUdpListener::EventCallback(unsigned events) noexcept
try {
	if (events & event.ERROR)
		throw MakeErrno(event.GetSocket().GetError(),
				"Socket error");

	if ((events & event.HANGUP) != 0 &&
	    !handler.OnUdpHangup())
		return;

	if (!multi.Receive(GetSocket())) {
		handler.OnUdpDatagram(nullptr, nullptr, nullptr, -1);
		return;
	}

	for (auto &d : multi)
		if (!handler.OnUdpDatagram(d.payload,
					   d.fds,
					   d.address,
					   d.cred != nullptr
					   ? d.cred->uid
					   : -1))
			return;

	multi.Clear();
} catch (...) {
	/* unregister the SocketEvent, just in case the handler does
	   not destroy us */
	event.Cancel();

	handler.OnUdpError(std::current_exception());
}

void
MultiUdpListener::Reply(SocketAddress address,
			const void *data, std::size_t data_length)
{
	assert(event.IsDefined());

	ssize_t nbytes = sendto(event.GetSocket().Get(), data, data_length,
				MSG_DONTWAIT|MSG_NOSIGNAL,
				address.GetAddress(), address.GetSize());
	if (gcc_unlikely(nbytes < 0))
		throw MakeErrno("Failed to send UDP packet");

	if ((std::size_t)nbytes != data_length)
		throw std::runtime_error("Short send");
}
