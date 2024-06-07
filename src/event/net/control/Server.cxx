// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Server.hxx"
#include "Handler.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "net/control/Padding.hxx"
#include "net/SocketConfig.hxx"
#include "net/SocketAddress.hxx"
#include "net/SendMessage.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Iovec.hxx"
#include "util/ByteOrder.hxx"

namespace BengControl {

Server::Server(EventLoop &event_loop, UniqueSocketDescriptor s,
	       Handler &_handler) noexcept
	:handler(_handler), socket(event_loop, std::move(s), *this)
{
}

Server::Server(EventLoop &event_loop, Handler &_handler,
	       const SocketConfig &config)
	:Server(event_loop, config.Create(SOCK_DGRAM), _handler)
{
}

static void
control_server_decode(Server &control_server,
		      const void *data, size_t length,
		      std::span<UniqueFileDescriptor> fds,
		      SocketAddress address, int uid,
		      Handler &handler)
{
	/* verify the magic number */

	const uint32_t *magic = (const uint32_t *)data;

	if (length < sizeof(*magic) || FromBE32(*magic) != MAGIC)
		throw std::runtime_error("wrong magic");

	data = magic + 1;
	length -= sizeof(*magic);

	if (!IsSizePadded(length))
		throw FmtRuntimeError("odd control packet (length={})", length);

	/* now decode all commands */

	while (length > 0) {
		const auto *header = (const Header *)data;
		if (length < sizeof(*header))
			throw FmtRuntimeError("partial header (length={})",
					      length);

		size_t payload_length = FromBE16(header->length);
		const auto command = static_cast<Command>(FromBE16(header->command));

		data = header + 1;
		length -= sizeof(*header);

		const std::byte *payload = (const std::byte *)data;
		if (length < payload_length)
			throw FmtRuntimeError("partial payload (length={}, expected={})",
					      length, payload_length);

		/* this command is ok, pass it to the callback */

		handler.OnControlPacket(control_server, command,
					{payload_length > 0 ? payload : nullptr, payload_length},
					fds,
					address, uid);

		payload_length = PadSize(payload_length);

		data = payload + payload_length;
		length -= payload_length;
	}
}

bool
Server::OnUdpDatagram(std::span<const std::byte> payload,
		      std::span<UniqueFileDescriptor> fds,
		      SocketAddress address, int uid)
try {
	control_server_decode(*this, payload.data(), payload.size(),
			      fds, address, uid, handler);
	return true;
} catch (...) {
	handler.OnControlError(std::current_exception());
	return true;
}

void
Server::OnUdpError(std::exception_ptr ep) noexcept
{
	handler.OnControlError(ep);
}

void
Server::Reply(SocketAddress address,
	      Command command,
	      std::span<const std::byte> payload)
{
	const Header header{ToBE16(payload.size()), ToBE16(uint16_t(command))};

	const struct iovec v[] = {
		MakeIovecT(header),
		MakeIovec(payload),
	};

	SendMessage(socket.GetSocket(),
		    MessageHeader{v}.SetAddress(address),
		    MSG_DONTWAIT|MSG_NOSIGNAL);
}

} // namespace BengControl
