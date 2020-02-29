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

#include "Parser.hxx"
#include "AddressInfo.hxx"
#include "Resolver.hxx"
#include "AllocatedSocketAddress.hxx"

#include <stdexcept>

#include <netdb.h>

static constexpr struct addrinfo
MakeActiveHints() noexcept
{
	return MakeAddrInfo(AI_NUMERICHOST,
			    AF_UNSPEC, SOCK_STREAM);
}

static constexpr struct addrinfo
MakePassiveHints() noexcept
{
	return MakeAddrInfo(AI_NUMERICHOST|AI_PASSIVE,
			    AF_UNSPEC, SOCK_STREAM);
}

AllocatedSocketAddress
ParseSocketAddress(const char *p, int default_port, bool passive)
{
	if (*p == '/') {
		AllocatedSocketAddress address;
		address.SetLocal(p);
		return address;
	}

	if (*p == '@') {
#ifdef __linux__
		/* abstract unix domain socket */

		AllocatedSocketAddress address;
		address.SetLocal(p);
		return address;
#else
		/* Linux specific feature */
		throw std::runtime_error("Abstract sockets supported only on Linux");
#endif
	}

	static constexpr struct addrinfo hints = MakeActiveHints();
	static constexpr struct addrinfo passive_hints = MakePassiveHints();

	const auto ai = Resolve(p, default_port,
				passive ? &passive_hints : &hints);
	return AllocatedSocketAddress(ai.GetBest());
}
