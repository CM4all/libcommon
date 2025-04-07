// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "StringListPtr.hxx"
#include "util/IntrusiveList.hxx"

#include <avahi-common/address.h>

#include <cstdint>
#include <string>

class SocketAddress;

namespace Avahi {

/**
 * A service that will be published by class #Publisher.
 */
struct Service : IntrusiveListHook<> {
	std::string type;

	StringListPtr txt;

	AvahiIfIndex interface = AVAHI_IF_UNSPEC;
	AvahiProtocol protocol = AVAHI_PROTO_UNSPEC;

	uint16_t port;

	/**
	 * If this is false, then the service is not published.  You
	 * can change this field at any time and then call
	 * Publisher::UpdateServices() to publish the change.
	 */
	bool visible = true;

	Service(AvahiIfIndex _interface, AvahiProtocol _protocol,
		const char *_type, uint16_t _port) noexcept
		:type(_type),
		 interface(_interface), protocol(_protocol),
		 port(_port) {}

	/**
	 * @param v6only the value of IPV6_V6ONLY (if this describes
	 * an IPv6 address)
	 */
	Service(const char *_type, const char *_interface,
		SocketAddress address, bool v6only) noexcept;
};

} // namespace Avahi
