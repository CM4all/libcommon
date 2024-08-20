// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Client.hxx"
#include "Padding.hxx"
#include "translation/Protocol.hxx"
#include "net/RConnectSocket.hxx"
#include "net/SendMessage.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "net/MsgHdr.hxx"
#include "net/SocketError.hxx"
#include "net/IPv4Address.hxx"
#include "io/Iovec.hxx"
#include "util/ByteOrder.hxx"

namespace BengControl {

Client::Client(const char *host_and_port)
	:Client(ResolveConnectDatagramSocket(host_and_port,
					     DEFAULT_PORT)) {}

/**
 * Create a new datagram socket that is connected to the same address
 * as the specified one.  Returns an "undefined"
 * UniqueSocketDescriptor on error.
 */
static UniqueSocketDescriptor
CloneConnectedDatagramSocket(SocketDescriptor old_socket) noexcept
{
	const auto peer_address = old_socket.GetPeerAddress();
	if (!peer_address.IsDefined())
		return {};

	UniqueSocketDescriptor new_socket;
	if (!new_socket.Create(peer_address.GetFamily(), SOCK_DGRAM,
			       old_socket.GetProtocol()))
		return {};

	if (!new_socket.Connect(peer_address))
		return {};

	return new_socket;
}

void
Client::Send(Command cmd,
	     std::span<const std::byte> payload,
	     std::span<const FileDescriptor> fds) const
{
	static constexpr uint32_t magic = ToBE32(MAGIC);
	const Header header{ToBE16(payload.size()), ToBE16(uint16_t(cmd))};

	static constexpr uint8_t padding[3] = {0, 0, 0};

	const struct iovec v[] = {
		MakeIovecT(magic),
		MakeIovecT(header),
		MakeIovec(payload),
		MakeIovec(std::span{padding, PaddingSize(payload.size())}),
	};

	MessageHeader msg{std::span{v}};

	ScmRightsBuilder<1> b(msg);
	for (const auto &i : fds)
		b.push_back(i.Get());
	b.Finish(msg);

	try {
		SendMessage(socket, msg, 0);
	} catch (const std::system_error &e) {
		if (IsSocketError(e, ENETUNREACH)) {
			/* ENETUNREACH can happen when the outgoing
			   network interface gets a new address which
			   invalidates our socket which was
			   (implicitly) bound to the old address; to
			   fix this, we create a new socket, connect
			   it (which binds it to the new address) */

			auto new_socket = CloneConnectedDatagramSocket(socket);
			if (new_socket.IsDefined() &&
			    new_socket.ToFileDescriptor().Duplicate(socket.ToFileDescriptor())) {
				SendMessage(socket, msg, 0);
				return;
			}
		}
		throw;
	}
}

void
Client::Send(std::span<const std::byte> payload) const
{
	auto nbytes = socket.Send(payload);
	if (nbytes < 0)
		throw MakeSocketError("send() failed");

}

std::pair<Command, std::string>
Client::Receive() const
{
	int result = socket.WaitReadable(10000);
	if (result < 0)
		throw MakeSocketError("poll() failed");

	if (result == 0)
		throw std::runtime_error("Timeout");

	Header header;
	char payload[4096];

	struct iovec v[] = {
		MakeIovecT(header),
		MakeIovecT(payload),
	};

	auto msg = MakeMsgHdr(v);

	auto nbytes = socket.Receive(msg);
	if (nbytes < 0)
		throw MakeSocketError("recvmsg() failed");

	if (size_t(nbytes) < sizeof(header))
		throw std::runtime_error("Short receive");

	size_t payload_length = FromBE16(header.length);
	if (sizeof(header) + payload_length > size_t(nbytes))
		throw std::runtime_error("Truncated datagram");

	return std::make_pair(static_cast<Command>(FromBE16(header.command)),
			      std::string(payload, payload_length));
}

std::string
Client::MakeTcacheInvalidate(TranslationCommand cmd,
			     std::span<const std::byte> payload) noexcept
{
	TranslationHeader h;
	h.length = ToBE16(payload.size());
	h.command = static_cast<TranslationCommand>(ToBE16(uint16_t(cmd)));

	std::string result;
	result.append((const char *)&h, sizeof(h));
	if (!payload.empty()) {
		result.append((const char *)payload.data(), payload.size());
		result.append(PaddingSize(payload.size()), '\0');
	}

	return result;
}

} // namespace BengControl
