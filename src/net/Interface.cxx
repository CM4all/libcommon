// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Interface.hxx"
#include "SocketAddress.hxx"
#include "util/ScopeExit.hxx"

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>

[[gnu::pure]]
static bool
Match(const struct sockaddr_in &a,
      const struct sockaddr_in &b) noexcept
{
	return memcmp(&a.sin_addr, &b.sin_addr, sizeof(a.sin_addr)) == 0;
}

[[gnu::pure]]
static bool
Match(const struct sockaddr_in6 &a,
      const struct sockaddr_in6 &b) noexcept
{
	return memcmp(&a.sin6_addr, &b.sin6_addr, sizeof(a.sin6_addr)) == 0;
}

[[gnu::pure]]
static bool
Match(const struct sockaddr &a,
      SocketAddress b) noexcept
{
	if (a.sa_family != b.GetFamily())
		return false;

	switch (a.sa_family) {
	case AF_INET:
		return Match((const struct sockaddr_in &)a,
			     b.CastTo<struct sockaddr_in>());

	case AF_INET6:
		return Match((const struct sockaddr_in6 &)a,
			     b.CastTo<struct sockaddr_in6>());

	default:
		/* other address families are unsupported */
		return false;
	}
}

[[gnu::pure]]
static bool
Match(const struct ifaddrs &ifa,
      SocketAddress address) noexcept
{
	return ifa.ifa_addr != nullptr && Match(*ifa.ifa_addr, address);
}

unsigned
FindNetworkInterface(SocketAddress address) noexcept
{
	struct ifaddrs *ifa;
	if (getifaddrs(&ifa) != 0)
		return 0;

	AtScopeExit(ifa) {
		freeifaddrs(ifa);
	};

	for (; ifa != nullptr; ifa = ifa->ifa_next) {
		if (Match(*ifa, address))
			return if_nametoindex(ifa->ifa_name);
	}

	return 0;
}

