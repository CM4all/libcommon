// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "RConnectSocket.hxx"
#include "Resolver.hxx"
#include "AllocatedSocketAddress.hxx"
#include "AddressInfo.hxx"
#include "Parser.hxx"
#include "SocketError.hxx"
#include "UniqueSocketDescriptor.hxx"

#include <stdexcept>

static void
ConnectWait(SocketDescriptor s, const SocketAddress address,
	    std::chrono::duration<int, std::milli> timeout)
{
	if (s.Connect(address))
		return;

	const auto connect_error = GetSocketError();
	if (!IsSocketErrorConnectWouldBlock(connect_error))
		throw MakeSocketError(connect_error, "Failed to connect");

	int w = s.WaitWritable(timeout.count());
	if (w < 0)
		throw MakeSocketError("Connect wait error");
	else if (w == 0)
		throw std::runtime_error("Connect timeout");

	int err = s.GetError();
	if (err != 0)
		throw MakeSocketError(err, "Failed to connect");
}

UniqueSocketDescriptor
ResolveConnectSocket(const char *host_and_port, int default_port,
		     const struct addrinfo &hints,
		     std::chrono::duration<int, std::milli> timeout)
{
	const auto ail = Resolve(host_and_port, default_port, &hints);
	const auto &ai = ail.GetBest();

	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(ai.GetFamily(), ai.GetType(), ai.GetProtocol()))
		throw MakeSocketError("Failed to create socket");

	ConnectWait(s, ai, timeout);
	return s;
}

static UniqueSocketDescriptor
ParseConnectSocket(const char *host_and_port, int default_port,
		   int socktype,
		   std::chrono::duration<int, std::milli> timeout)
{
	const auto address = ParseSocketAddress(host_and_port,
						default_port, false);
	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(address.GetFamily(), socktype, 0))
		throw MakeSocketError("Failed to create socket");

	ConnectWait(s, address, timeout);
	return s;
}

static UniqueSocketDescriptor
ResolveConnectSocket(const char *host_and_port, int default_port,
		     int socktype,
		     std::chrono::duration<int, std::milli> timeout)
{
	if (*host_and_port == '/' || *host_and_port == '@')
		return ParseConnectSocket(host_and_port, default_port,
					  socktype, timeout);

	return ResolveConnectSocket(host_and_port, default_port,
				    MakeAddrInfo(AI_ADDRCONFIG,
						 AF_UNSPEC, socktype),
				    timeout);
}

UniqueSocketDescriptor
ResolveConnectStreamSocket(const char *host_and_port, int default_port,
			   std::chrono::duration<int, std::milli> timeout)
{
	return ResolveConnectSocket(host_and_port, default_port, SOCK_STREAM,
				    timeout);
}

UniqueSocketDescriptor
ResolveConnectDatagramSocket(const char *host_and_port, int default_port)
{
	/* hard-coded zero timeout, because "connecting" a datagram socket
	   cannot block */

	return ResolveConnectSocket(host_and_port, default_port, SOCK_DGRAM,
				    std::chrono::seconds(0));
}
