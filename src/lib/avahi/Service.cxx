/*
 * Copyright 2007-2022 CM4all GmbH
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
