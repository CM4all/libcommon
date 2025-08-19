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
};

} // namespace Avahi
