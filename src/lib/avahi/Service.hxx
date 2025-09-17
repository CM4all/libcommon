// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "StringListPtr.hxx"
#include "util/IntrusiveList.hxx"

#include <avahi-common/address.h>

#include <cstdint>
#include <string>

class SocketAddress;

namespace Avahi {

struct ServiceConfig;

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

	/**
	 * Create a #Service instance from a #ServiceConfig.
	 *
	 * @param config a ServiceConfig whose IsEnabled() must be
	 * true
	 *
	 * @param interface2 the network interface the listener socket
	 * was (explicitly) bound to (may be nullptr); this will be
	 * used if there is no "zeroconf_interface" setting
	 *
	 * @param bound_address the address where the listener socket
	 * is bound; this will be used to determine the port and
	 * optionally the network interface and the protocol (if none
	 * was given in the configuration)
	 *
	 * @param v6only true if the listener socket was bound using
	 * the "v6only" option
	 *
	 * @param arch add an "arch" TXT record?
	 */
	Service(const ServiceConfig &config, const char *interface2,
		SocketAddress bound_address, bool v6only,
		bool arch=true) noexcept;
};

} // namespace Avahi
