// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MultiUdpListener.hxx"
#include "UdpHandler.hxx"
#include "system/Error.hxx"
#include "net/UniqueSocketDescriptor.hxx"

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
			std::span<const std::byte> payload)
{
	assert(event.IsDefined());

	ssize_t nbytes = sendto(event.GetSocket().Get(),
				payload.data(), payload.size(),
				MSG_DONTWAIT|MSG_NOSIGNAL,
				address.GetAddress(), address.GetSize());
	if (nbytes < 0) [[unlikely]]
		throw MakeErrno("Failed to send UDP packet");

	if ((std::size_t)nbytes != payload.size()) [[unlikely]]
		throw std::runtime_error("Short send");
}
