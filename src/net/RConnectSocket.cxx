/*
 * Copyright 2007-2017 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "RConnectSocket.hxx"
#include "Resolver.hxx"
#include "AllocatedSocketAddress.hxx"
#include "AddressInfo.hxx"
#include "Parser.hxx"
#include "UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"

static void
ConnectWait(SocketDescriptor s, const SocketAddress address,
	    std::chrono::duration<int, std::milli> timeout)
{
	if (s.Connect(address))
		return;

	if (errno != EINPROGRESS)
		throw MakeErrno("Failed to connect");

	int w = s.WaitWritable(timeout.count());
	if (w < 0)
		throw MakeErrno("Connect wait error");
	else if (w == 0)
		throw std::runtime_error("Connect timeout");

	int err = s.GetError();
	if (err != 0)
		throw MakeErrno(err, "Failed to connect");
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
		throw MakeErrno("Failed to create socket");

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
		throw MakeErrno("Failed to create socket");

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

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = socktype;

	return ResolveConnectSocket(host_and_port, default_port, hints, timeout);
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
