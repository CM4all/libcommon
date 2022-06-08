/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "UdpListener.hxx"
#include "UdpHandler.hxx"
#include "net/SocketAddress.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/Compiler.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

UdpListener::UdpListener(EventLoop &event_loop, UniqueSocketDescriptor _fd,
			 UdpHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(EventCallback), _fd.Release()),
	 handler(_handler)
{
	event.ScheduleRead();
}

UdpListener::~UdpListener() noexcept
{
	event.Close();
}

bool
UdpListener::ReceiveAll()
try {
	while (true) {
		try {
			if (!ReceiveOne())
				return false;
		} catch (const std::system_error &e) {
			if (IsErrno(e, EAGAIN))
				/* no more pending datagrams */
				return true;

			throw;
		}
	}
} catch (...) {
	event.Cancel();
	throw;
}

bool
UdpListener::ReceiveOne()
{
	ReceiveMessageBuffer<4096, 1024> buffer;
	auto result = ReceiveMessage(GetSocket(), buffer, MSG_DONTWAIT);
	int uid = result.cred != nullptr
		? result.cred->uid
		: -1;

	std::span<UniqueFileDescriptor> fds{};
	if (!result.fds.empty())
		fds = result.fds;

	return handler.OnUdpDatagram(result.payload,
				     fds,
				     result.address,
				     uid);
}

void
UdpListener::EventCallback(unsigned events) noexcept
try {
	if (events & event.ERROR)
		throw MakeErrno(event.GetSocket().GetError(),
				"Socket error");

	if ((events & event.HANGUP) != 0 &&
	    !handler.OnUdpHangup())
		return;

	ReceiveOne();
} catch (...) {
	/* unregister the SocketEvent, just in case the handler does
	   not destroy us */
	event.Cancel();

	handler.OnUdpError(std::current_exception());
}

void
UdpListener::Reply(SocketAddress address,
		   std::span<const std::byte> payload)
{
	assert(event.IsDefined());

	ssize_t nbytes = sendto(GetSocket().Get(),
				payload.data(), payload.size_bytes(),
				MSG_DONTWAIT|MSG_NOSIGNAL,
				address.GetAddress(), address.GetSize());
	if (gcc_unlikely(nbytes < 0))
		throw MakeErrno("Failed to send UDP packet");

	if ((std::size_t)nbytes != payload.size_bytes())
		throw std::runtime_error("Short send");
}
