// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "UdpListener.hxx"
#include "UdpHandler.hxx"
#include "net/SocketAddress.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketError.hxx"

#include <assert.h>
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
			if (IsSocketErrorReceiveWouldBlock(e))
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
		throw MakeSocketError(event.GetSocket().GetError(),
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
	if (nbytes < 0) [[unlikely]]
		throw MakeSocketError("Failed to send UDP packet");

	if ((std::size_t)nbytes != payload.size_bytes()) [[unlikely]]
		throw std::runtime_error("Short send");
}
