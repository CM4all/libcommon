// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
ParseSocketAddress(const char *p, int default_port,
		   const struct addrinfo &hints)
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

	const auto ai = Resolve(p, default_port, &hints);
	return AllocatedSocketAddress(ai.GetBest());
}

AllocatedSocketAddress
ParseSocketAddress(const char *p, int default_port, bool passive)
{
	static constexpr struct addrinfo active_hints = MakeActiveHints();
	static constexpr struct addrinfo passive_hints = MakePassiveHints();

	return ParseSocketAddress(p, default_port,
				  passive ? passive_hints : active_hints);
}
