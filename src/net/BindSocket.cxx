// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "BindSocket.hxx"
#include "AddressInfo.hxx"
#include "IPv6Address.hxx"
#include "SocketAddress.hxx"
#include "SocketError.hxx"
#include "UniqueSocketDescriptor.hxx"

UniqueSocketDescriptor
BindSocket(int domain, int type, int protocol, SocketAddress address)
{
	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(domain, type, protocol))
		throw MakeSocketError("Failed to create socket");

	if (address.IsInet() && type == SOCK_STREAM)
		/* always set SO_REUSEADDR for TCP sockets to allow
		   quick restarts */
		s.SetReuseAddress();

	if (!s.Bind(address))
		throw MakeSocketError("Failed to bind");

	return s;
}

UniqueSocketDescriptor
BindSocket(int type, SocketAddress address)
{
	return BindSocket(address.GetFamily(), type, 0, address);
}

UniqueSocketDescriptor
BindSocket(const AddressInfo &ai)
{
	return BindSocket(ai.GetFamily(), ai.GetType(), ai.GetProtocol(), ai);
}

UniqueSocketDescriptor
BindLoopback(int type, unsigned port)
{
	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(AF_INET6, SOCK_STREAM, 0))
		throw MakeSocketError("Failed to create socket");

	// TODO do we need a fallback to IPv4?

	/* always set SO_REUSEADDR for TCP sockets to allow quick
	   restarts */
	if (type == SOCK_STREAM)
		s.SetReuseAddress();

	s.SetV6Only(false);

	if (!s.SetBindToDevice("lo"))
		throw MakeSocketError("Failed to bind the loopback device");

	if (!s.Bind(IPv6Address(port)))
		throw MakeSocketError("Failed to bind");

	return s;
}

UniqueSocketDescriptor
BindPort(int type, unsigned port)
{
	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(AF_INET6, SOCK_STREAM, 0))
		throw MakeSocketError("Failed to create socket");

	// TODO do we need a fallback to IPv4?

	/* always set SO_REUSEADDR for TCP sockets to allow quick
	   restarts */
	if (type == SOCK_STREAM)
		s.SetReuseAddress();

	s.SetV6Only(false);

	if (!s.Bind(IPv6Address(port)))
		throw MakeSocketError("Failed to bind");

	return s;
}
