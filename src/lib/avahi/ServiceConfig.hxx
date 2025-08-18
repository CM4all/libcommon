// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <avahi-common/address.h>

#include <memory>
#include <string>

class SocketAddress;
class FileLineParser;

namespace Avahi {

struct Service;

struct ServiceConfig {
	std::string service, domain, interface;

	/**
	 * The weight published via Zeroconf.  Negative value means
	 * don't publish a weight (peers will assume the default
	 * weight, i.e. 1.0).
	 */
	float weight = -1;

	AvahiProtocol protocol = AVAHI_PROTO_UNSPEC;

	bool IsEnabled() const noexcept {
		return !service.empty();
	}

	/**
	 * Parse a configuration file line.
	 *
	 * Throws on error.
	 *
	 * @return false if the word was not recognized
	 */
	bool ParseLine(const char *word, FileLineParser &line);

	/**
	 * Check whether the configuration is formally correct.
	 * Throws on error.
	 */
	void Check() const;

	/**
	 * Create a #Service instance for this configuration.
	 *
	 * IsEnabled() must be true.
	 *
	 * Throws on error.
	 *
	 * @param interface2 the network interface the listener socket
	 * was (explicitly) bound to (may be nullptr); this will be
	 * used if there is no "zeroconf_interface" setting
	 *
	 * @param local_address the address where the listener socket
	 * is bound; this will be used to determine the port and
	 * optionally the network interface and the protocol (if none
	 * was given in the configuration)
	 *
	 * @param v6only true if the listener socket was bound using
	 * the "v6only" option
	 */
	std::unique_ptr<Service> Create(const char *interface2,
					SocketAddress local_address,
					bool v6only) const noexcept;
};

} // namespace Avahi
