// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Parser.hxx"
#include "AddressInfo.hxx"
#include "Resolver.hxx"
#include "AllocatedSocketAddress.hxx"

#include <stdexcept>

#include <netdb.h>

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
	static constexpr struct addrinfo active_hints{
		.ai_flags = AI_NUMERICHOST,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	static constexpr struct addrinfo passive_hints{
		.ai_flags = AI_NUMERICHOST|AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	return ParseSocketAddress(p, default_port,
				  passive ? passive_hints : active_hints);
}
