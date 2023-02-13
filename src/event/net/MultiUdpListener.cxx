// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
		handler.OnUdpDatagram({}, {}, nullptr, -1);
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
