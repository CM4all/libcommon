/*
 * Copyright 2007-2021 CM4all GmbH
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

