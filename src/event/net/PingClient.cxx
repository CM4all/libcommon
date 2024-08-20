// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PingClient.hxx"
#include "net/InetChecksum.hxx"
#include "net/IPv4Address.hxx"
#include "net/MsgHdr.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketError.hxx"
#include "net/SendMessage.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Iovec.hxx"
#include "util/ByteOrder.hxx"
#include "util/SpanCast.hxx"

#include <cassert>
#include <stdexcept>

#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>

PingClient::PingClient(EventLoop &event_loop,
		       PingClientHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(EventCallback)),
	 handler(_handler)
{
}

inline void
PingClient::ScheduleRead() noexcept
{
	event.ScheduleRead();
}

static bool
parse_reply(const struct icmphdr &header, const std::byte *payload,
	    size_t nbytes, uint16_t ident) noexcept
{
	if (nbytes < sizeof(header))
		return false;

	return header.type == ICMP_ECHOREPLY && header.un.echo.id == ident &&
	       InetChecksum{}.UpdateT(header).Update({payload, nbytes - sizeof(header)}).Finish() == 0;
}

inline void
PingClient::Read() noexcept
{
	struct icmphdr header;
	std::byte payload[8];

	std::array iov{MakeIovecT(header), MakeIovec(payload)};

	auto msg = MakeMsgHdr(iov);

	int cc = event.GetSocket().Receive(msg, MSG_DONTWAIT);
	if (cc >= 0) {
		if (parse_reply(header, payload, cc, ident)) {
			event.Close();
			handler.PingResponse();
		}
	} else if (const auto e = GetSocketError(); !IsSocketErrorReceiveWouldBlock(e)) {
		event.Close();
		handler.PingError(std::make_exception_ptr(MakeSocketError(e, "Failed to receive ping reply")));
	}
}


/*
 * libevent callback
 *
 */

inline void
PingClient::EventCallback(unsigned) noexcept
{
	assert(event.IsDefined());

	Read();
}

/*
 * constructor
 *
 */

bool
PingClient::IsAvailable() noexcept
{
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	if (fd < 0)
		return false;
	close(fd);
	return true;
}

static UniqueSocketDescriptor
CreateIcmp()
{
	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(AF_INET, SOCK_DGRAM, IPPROTO_ICMP))
		throw MakeSocketError("Failed to create ICMP socket");

	return fd;
}

static uint16_t
MakeIdent(SocketDescriptor fd)
{
	if (!fd.Bind(IPv4Address(0)))
		throw MakeSocketError("Failed to bind ICMP socket");

	const auto address = fd.GetLocalAddress();
	if (!address.IsDefined())
		throw MakeSocketError("Failed to inspect ICMP socket");

	switch (address.GetFamily()) {
	case AF_INET:
		return IPv4Address::Cast(address).GetPortBE();

	default:
		throw std::runtime_error{"Unsupported address family"};
	}
}

static void
SendPing(SocketDescriptor fd, SocketAddress address, uint16_t ident)
{
	static constexpr std::byte payload[8]{};

	struct icmphdr header{
		.type = ICMP_ECHO,
		.un = {
			.echo = {
				.id = ident,
				.sequence = ToBE16(1),
			},
		},
	};

	header.checksum = InetChecksum{}.UpdateT(header).Update(payload).Finish();

	const std::array iov{MakeIovecT(header), MakeIovec(payload)};

	SendMessage(fd,
		    MessageHeader(iov)
		    .SetAddress(address), 0);
}

void
PingClient::Start(SocketAddress address) noexcept
{
	try {
		event.Open(CreateIcmp().Release());
		ident = MakeIdent(event.GetSocket());
		SendPing(event.GetSocket(), address, ident);
	} catch (...) {
		handler.PingError(std::current_exception());
		return;
	}

	ScheduleRead();
}
