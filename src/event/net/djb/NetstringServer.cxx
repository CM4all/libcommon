// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "NetstringServer.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <stdexcept>
#include <utility> // for std::unreachable()

static constexpr auto busy_timeout = std::chrono::seconds(5);

NetstringServer::NetstringServer(EventLoop &event_loop,
				 UniqueSocketDescriptor fd,
				 size_t max_size) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnEvent), fd.Release()),
	 timeout_event(event_loop, BIND_THIS_METHOD(OnTimeout)),
	 input(max_size)
{
	event.ScheduleRead();
	timeout_event.Schedule(busy_timeout);
}

NetstringServer::~NetstringServer() noexcept
{
	event.Close();
}

bool
NetstringServer::SendResponse(std::span<const std::byte> response) noexcept
try {
	std::list<std::span<const std::byte>> list{response};
	generator(list);
	for (const auto &i : list)
		write.Push(i);

	switch (write.Write(GetSocket().ToFileDescriptor())) {
	case MultiWriteBuffer::Result::MORE:
		throw std::runtime_error("short write");

	case MultiWriteBuffer::Result::FINISHED:
		return true;
	}

	std::unreachable();
} catch (...) {
	OnError(std::current_exception());
	return false;
}

void
NetstringServer::OnEvent(unsigned flags) noexcept
try {
	if (flags & SocketEvent::ERROR)
		throw MakeSocketError(GetSocket().GetError(), "Socket error");

	if (flags & SocketEvent::HANGUP) {
		OnDisconnect();
		return;
	}

	if (IsRequestReceived()) {
		/* TODO: was garbage received or did the peer just
		   close the socket?  Maybe use EPOLLRDHUP? */
		OnDisconnect();
		return;
	}

	switch (input.Receive(GetSocket().ToFileDescriptor())) {
	case NetstringInput::Result::MORE:
		timeout_event.Schedule(busy_timeout);
		break;

	case NetstringInput::Result::CLOSED:
		OnDisconnect();
		break;

	case NetstringInput::Result::FINISHED:
		timeout_event.Cancel();
		OnRequest(std::move(input.GetValue()));
		break;
	}
} catch (...) {
	OnError(std::current_exception());
}

void
NetstringServer::OnTimeout() noexcept
{
	OnDisconnect();
}
