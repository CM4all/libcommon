// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Service.hxx"
#include "net/SocketAddress.hxx"
#include "net/IPv6Address.hxx"
#include "net/Interface.hxx"

#include <net/if.h>

namespace Avahi {

Service::Service(const char *_type, const char *_interface,
		 SocketAddress address, bool v6only) noexcept
	:type(_type), port(address.GetPort())
{
	unsigned i = 0;
	if (_interface != nullptr)
		i = if_nametoindex(_interface);

	if (i == 0)
		i = FindNetworkInterface(address);

	if (i > 0)
		interface = AvahiIfIndex(i);

	switch (address.GetFamily()) {
	case AF_INET:
		protocol = AVAHI_PROTO_INET;
		break;

	case AF_INET6:
		/* don't restrict to AVAHI_PROTO_INET6 if IPv4
		   connections are possible (i.e. this is a wildcard
		   listener and v6only disabled) */
		if (v6only || !IPv6Address::Cast(address).IsAny())
			protocol = AVAHI_PROTO_INET6;
		break;
	}
}

} // namespace Avahi
