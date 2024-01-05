// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "RBindSocket.hxx"
#include "AddressInfo.hxx"
#include "AllocatedSocketAddress.hxx"
#include "Parser.hxx"
#include "Resolver.hxx"
#include "SocketError.hxx"
#include "UniqueSocketDescriptor.hxx"

UniqueSocketDescriptor
ResolveBindSocket(const char *host_and_port, int default_port,
		  const struct addrinfo &hints)
{
	const auto ail = Resolve(host_and_port, default_port, &hints);
	const auto &ai = ail.GetBest();

	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(ai.GetFamily(), ai.GetType(), ai.GetProtocol()))
		throw MakeSocketError("Failed to create socket");

	if (ai.IsTCP())
		/* always set SO_REUSEADDR for TCP sockets to allow
		   quick restarts */
		s.SetReuseAddress();

	if (!s.Bind(ai))
		throw MakeSocketError("Failed to bind");

	return s;
}

static UniqueSocketDescriptor
ParseBindSocket(const char *host_and_port, int default_port, int socktype)
{
	const auto address = ParseSocketAddress(host_and_port,
						default_port, true);

	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(address.GetFamily(), socktype, 0))
		throw MakeSocketError("Failed to create socket");

	if (address.IsInet() && socktype == SOCK_STREAM)
		/* always set SO_REUSEADDR for TCP sockets to allow
		   quick restarts */
		s.SetReuseAddress();

	if (!s.Bind(address))
		throw MakeSocketError("Failed to bind");

	return s;
}

static UniqueSocketDescriptor
ResolveBindSocket(const char *host_and_port, int default_port, int socktype)
{
	if (*host_and_port == '/' || *host_and_port == '@') {
		if (*host_and_port == '/')
			unlink(host_and_port);

		return ParseBindSocket(host_and_port, default_port, socktype);
	}

	return ResolveBindSocket(host_and_port, default_port,
				 MakeAddrInfo(AI_ADDRCONFIG|AI_PASSIVE,
					      AF_UNSPEC, socktype));
}

UniqueSocketDescriptor
ResolveBindStreamSocket(const char *host_and_port, int default_port)
{
	return ResolveBindSocket(host_and_port, default_port, SOCK_STREAM);
}

UniqueSocketDescriptor
ResolveBindDatagramSocket(const char *host_and_port, int default_port)
{
	return ResolveBindSocket(host_and_port, default_port, SOCK_DGRAM);
}
