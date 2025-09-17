// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Service.hxx"
#include "ServiceConfig.hxx"
#include "Arch.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "net/SocketAddress.hxx"
#include "net/IPv6Address.hxx"
#include "net/Interface.hxx"

#include <net/if.h>

using std::string_view_literals::operator""sv;

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

Service::Service(const ServiceConfig &config, const char *interface2,
		 SocketAddress bound_address, bool v6only,
		 bool arch) noexcept
	:Service(config.service.c_str(),
		 !config.interface.empty() ? config.interface.c_str() : interface2,
		 bound_address, v6only)
{
	assert(config.IsEnabled());

	if (config.protocol != AVAHI_PROTO_UNSPEC)
		protocol = config.protocol;

	AvahiStringList *t = nullptr;

	if (arch)
		t = Avahi::AddArchTxt(t);

	if (config.weight >= 0)
		t = avahi_string_list_add_pair(t, "weight",
					       FmtBuffer<64>("{}"sv, config.weight));

	txt.reset(t);
}

} // namespace Avahi
