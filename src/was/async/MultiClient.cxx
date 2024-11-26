// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MultiClient.hxx"
#include "Socket.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "net/SendMessage.hxx"
#include "net/SocketDescriptor.hxx"
#include "net/SocketError.hxx"
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

/**
 * Send a #MULTI_WAS_COMMAND_NEW datagram on the given Multi-WAS
 * client socket.
 *
 * Throws on error.
 */
static void
SendMultiNew(SocketDescriptor s, WasSocket &&socket)
{
	static constexpr auto header =
		MakeMultiWasHeader(MULTI_WAS_COMMAND_NEW, 0);
	const struct iovec v[] = {MakeIovecT(header)};
	MessageHeader msg{v};

	ScmRightsBuilder<3> b(msg);
	b.push_back(socket.control.Get());
	b.push_back(socket.input.Get());
	b.push_back(socket.output.Get());
	b.Finish(msg);

	SendMessage(s, msg, MSG_NOSIGNAL|MSG_DONTWAIT);
}

MultiClient::MultiClient(EventLoop &event_loop, UniqueSocketDescriptor socket,
			 MultiClientHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnSocketReady),
	       socket.Release()),
	 handler(_handler)
{
	event.ScheduleRead();
}

inline void
MultiClient::Connect(WasSocket &&socket)
{
	Was::SendMultiNew(event.GetSocket(), std::move(socket));
}

WasSocket
MultiClient::Connect()
{
	auto [result, for_child] = WasSocket::CreatePair();
	Connect(std::move(for_child));

	result.input.SetNonBlocking();
	result.output.SetNonBlocking();
	return std::move(result);
}

void
MultiClient::OnSocketReady(unsigned events) noexcept
try {
	if (events & SocketEvent::ERROR)
		throw MakeSocketError(event.GetSocket().GetError(),
				      "Error on MultiWAS socket");

	if (events & SocketEvent::HANGUP) {
		event.Close();
		handler.OnMultiClientDisconnect();
		return;
	}

	throw std::runtime_error("Unexpected data on MultiWAS socket");
} catch (...) {
	event.Close();
	handler.OnMultiClientError(std::current_exception());
}

} // namespace Was
